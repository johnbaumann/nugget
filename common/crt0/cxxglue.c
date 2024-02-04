/*

MIT License

Copyright (c) 2021 PCSX-Redux authors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

/* Properly using C++ on the PSX requires a form of the libstdc++ that's
typically called "freestanding", which isn't really buildable at the moment.
See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=100057 for details.
This means however that using a full linux compiler such as the Ubuntu
mipsel compiler package will bring in a libstdc++ that utilizes all of
the existing libc. Properly filling in portions of the required ABI, while
avoiding the bits that won't work can be tricky, but is doable. Using a
freestanding compiler won't have the libstdc++, and this wouldn't work. */

#include <stdatomic.h>
#include <stddef.h>

#include "common/hardware/pcsxhw.h"

typedef void (*fptr)();

extern fptr __preinit_array_start[] __attribute__((weak));
extern fptr __preinit_array_end[] __attribute__((weak));
extern fptr __init_array_start[] __attribute__((weak));
extern fptr __init_array_end[] __attribute__((weak));

void main();

void cxxmain() {
    size_t count, i;

    count = __preinit_array_end - __preinit_array_start;
    for (i = 0; i < count; i++) {
        fptr f = __preinit_array_start[i];
        if (f) {
            f();
        }
    }

    count = __init_array_end - __init_array_start;
    for (i = 0; i < count; i++) {
        fptr f = __init_array_start[i];
        if (f) {
            f();
        }
    }

    main();
}

__attribute__((weak)) void abort() {
    pcsx_debugbreak();
    // TODO: make this better
    while (1)
        ;
}

// This will be called if a pure virtual function is called, usually mistakenly calling
// a method after its object got deleted, because that's the only way this happens.
__attribute__((weak)) void __cxa_pure_virtual() { abort(); }

// All of these (and probably more) will be called one way or another through
// the normal C++ ABI. These are declared weak here so they can be overriden
// by an actual proper implementation that'd do what they should do.
__attribute__((weak)) void* __builtin_new(size_t size) { abort(); }
__attribute__((weak)) void __builtin_delete(void* ptr) { abort(); }
// void * operator new(unsigned int);
__attribute__((weak)) void* _Znwj(unsigned int size) { abort(); }
// void * operator new[](unsigned int);
__attribute__((weak)) void* _Znaj(unsigned int size) { abort(); }
// void operator delete(void*);
__attribute__((weak)) void _ZdlPv(void* ptr) { abort(); }
// void operator delete[](void*);
__attribute__((weak)) void _ZdaPv(void* ptr) { abort(); }
// void operator delete(void*, unsigned int);
__attribute__((weak)) void _ZdlPvj(void* ptr, unsigned int size) { abort(); }
// void operator delete[](void*, unsigned int);
__attribute__((weak)) void _ZdaPvj(void* ptr, unsigned int size) { abort(); }

/*

  In order to use new / delete, the following must be defined somewhere, but due
  to the fact there's no good memory allocation story for the PS1 at start time,
  it's pretty difficult to define something that'd be one size fits all. It's
  best to drop these in your own project with your own memory allocation functions.

void * __builtin_new(size_t size) { return malloc(size); }
void __builtin_delete(void * ptr) { free(ptr); }
void * _Znwj(unsigned int size) { return malloc(size); }
void * _Znaj(unsigned int size) { return malloc(size); }
void _ZdlPv(void * ptr) { free(ptr); }
void _ZdaPv(void * ptr) { free(ptr); }
void _ZdlPvj(void * ptr, unsigned int size) { free(ptr); }
void _ZdaPvj(void * ptr, unsigned int size) { free(ptr); }

  One way to make this all work would be to have the following snippet to
  initialize the heap through the bios or any other mean before the memory
  allocation functions can be safely used:

static uint8_t global_heap[HEAPSIZE];
static void init_heap_wrapper() {
    InitHeap(global_heap, HEAPSIZE);
}

__attribute__((section(".preinit_array"))) static fptr pi_heap[] = {
    init_heap_wrapper
};

*/

// we're not going to care about exit cleanup
__attribute__((weak)) void __cxa_atexit(void (*func)(void*), void* arg, void* dso_handle) {}

// no, we're not going to have shared libraries
__attribute__((weak)) void* __dso_handle = NULL;

// These are the 4 essential functions expected by gcc for a naked build.
// The implementation of memcpy is in memory.s, and the rest are here.
// The weak attribute is used to allow the user to override these functions
// with their own implementation if they so desire. The memory.s implementation
// is a simple byte copy, and is not optimized for speed. The file also contains
// a faster but bigger implementation that can be used instead, called
// __wrap_memcpy. The user can override memcpy with __wrap_memcpy to use it using
// the -Wl,--wrap=memcpy switch to the linker using LDFLAGS.

void* memcpy(void* s1_, const void* s2_, size_t n);

__attribute__((weak)) void* memmove(void* s1_, const void* s2_, size_t n) {
    uint8_t* s1 = (uint8_t*)s1_;
    const uint8_t* s2 = (uint8_t*)s2_;
    size_t i;

    uint8_t* e1 = s1 + n;
    const uint8_t* e2 = s2 + n;

    if ((s1 <= s2) && (s2 <= e1) || ((s2 <= s1) && (s1 <= e2))) {
        if (s1 < s2) {
            for (i = 0; i < n; i++) *s1++ = *s2++;
        } else if (s1 > s2) {
            s1 += n;
            s2 += n;
            for (i = 0; i < n; i++) *--s1 = *--s2;
        }
    } else {
        return memcpy(s1_, s2_, n);
    }

    return s1_;
}

__attribute__((weak)) int memcmp(const void* s1_, const void* s2_, size_t n) {
    uint8_t* s1 = (uint8_t*)s1_;
    const uint8_t* s2 = (uint8_t*)s2_;
    size_t i;

    for (i = 0; i < n; i++, s1++, s2++) {
        if (*s1 < *s2) {
            return -1;
        } else if (*s1 > *s2) {
            return 1;
        }
    }

    return 0;
}

__attribute__((weak)) void* memset(void* s_, int c, size_t n) {
    uint8_t* s = (uint8_t*)s_;
    size_t i;

    for (i = 0; i < n; i++) *s++ = (uint8_t)c;

    return s_;
}

/* Some helpers to make sure we're not going to be preempted during object creation */
static inline uint32_t getCop0Status() {
    uint32_t r;
    asm("mfc0 %0, $12 ; nop" : "=r"(r));
    return r;
}

static inline void setCop0Status(uint32_t r) { asm("mtc0 %0, $12 ; nop" : : "r"(r)); }

static inline int fastEnterCriticalSection() {
    uint32_t sr = getCop0Status();
    setCop0Status(sr & ~0x401);
    return (sr & 0x401) == 0x401;
}

static inline void fastLeaveCriticalSection() {
    uint32_t sr = getCop0Status();
    sr |= 0x401;
    setCop0Status(sr);
}

/* In order to support inline object construction, we need to define at least
   these two functions that gcc is going to call. The guard object is technically
   a 64 bits value, but we're going to use it as an array of 2 32 bits values.

   The value is supposed to be initialized at 0 on process startup.

   Our first 32 bits value will be used as the full construction indicator.

   The second 32 bits value will be used as a marker to indicate the object is
   under construction, and do the multithreaded guard.

   The function is supposed to return 1 if the object requires to be constructed,
   or 0 if it was already constructed. We're also going to do some small amount
   of work to guard against "multithreaded" construction, although this really
   shouldn't happen, so we're simply going to abort in this case.
*/
__attribute__((weak)) int __cxa_guard_acquire(uint32_t* guardObject) {
    atomic_signal_fence(memory_order_consume);
    int needsToLeaveCS = fastEnterCriticalSection();
    // Object was already constructed, go ahead.
    if (guardObject[0]) {
        if (needsToLeaveCS) fastLeaveCriticalSection();
        atomic_signal_fence(memory_order_release);
        return 0;
    }

    // Object isn't already under construction, go ahead.
    if (guardObject[1] == 0) {
        guardObject[1] = 1;
        if (needsToLeaveCS) fastLeaveCriticalSection();
        atomic_signal_fence(memory_order_release);
        return 1;
    }

    abort();
}

__attribute__((weak)) void __cxa_guard_release(uint32_t* guardObject) {
    // Our object got constructed
    guardObject[0] = 1;
    // And is no longer under construction
    guardObject[1] = 0;
    atomic_signal_fence(memory_order_release);
}
