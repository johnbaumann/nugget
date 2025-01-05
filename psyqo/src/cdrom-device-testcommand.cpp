/*

MIT License

Copyright (c) 2022 PCSX-Redux authors

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

#include <EASTL/atomic.h>

#include "psyqo/cdrom-device.hh"
#include "psyqo/hardware/cdrom.hh"
#include "psyqo/kernel.hh"

namespace {

enum class TestActionState : uint8_t {
    IDLE,
    TEST,
};

class TestAction : public psyqo::CDRomDevice::Action<TestActionState> {
  public:
    TestAction() : Action("TestAction") {}

    void start(psyqo::CDRomDevice *device, eastl::function<void(bool)> &&callback) {
        psyqo::Kernel::assert(device->isIdle(), "CDRomDevice::test() called while another action is in progress");
        registerMe(device);
        setCallback(eastl::move(callback));
        setState(TestActionState::TEST);
        eastl::atomic_signal_fence(eastl::memory_order_release);
        psyqo::Hardware::CDRom::Command.send(psyqo::Hardware::CDRom::CDL::TEST, 0x50, 0xF1, 0xAD, 0xFF);
    }
    bool complete(const psyqo::CDRomDevice::Response &) override {
        setSuccess(true);
        return true;
    }
    bool acknowledge(const psyqo::CDRomDevice::Response &response) override {
        setSuccess(true);
        return true;
    }
};

TestAction s_testAction;

}  // namespace

void psyqo::CDRomDevice::test(eastl::function<void(bool)> &&callback) {
    Kernel::assert(m_callback == nullptr, "CDRomDevice::test called with pending action");
    s_testAction.start(this, eastl::move(callback));
}

psyqo::TaskQueue::Task psyqo::CDRomDevice::scheduleTest() {
    return TaskQueue::Task([this](auto task) { test([task](bool success) { task->complete(success); }); });
}

void psyqo::CDRomDevice::testBlocking(GPU &gpu) {
    Kernel::assert(m_callback == nullptr, "CDRomDevice::testBlocking called with pending action");
    bool success = false;
    {
        BlockingAction blocking(this, gpu);
        s_testAction.start(this, [&success](bool success_) { success = success_; });
    }
}