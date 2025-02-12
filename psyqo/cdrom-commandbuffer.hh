#pragma once

#include <stdint.h>

#include <concepts>
#include <type_traits>

template<typename T>
concept CDRomArgumentType = std::is_integral<T>::value;

struct CDRomCommandBuffer {
    template<CDRomArgumentType... T>
    void set(T... values) {
        size = sizeof...(values);
        recursiveSet(0, values...);
    }

    uint8_t buffer[16];
    uint8_t size = 0;
  private:


    void recursiveSet(uint8_t pos, uint8_t arg) {
        buffer[pos] = arg;
    }

    template<CDRomArgumentType... T>
    void recursiveSet(uint8_t pos, uint8_t arg, T... args) {
        buffer[pos] = arg;
        recursiveSet(pos + 1, args...);
    }
};
