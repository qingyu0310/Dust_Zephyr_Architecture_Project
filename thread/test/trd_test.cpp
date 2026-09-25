/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 测试线程 —— PWM 驱动 SG60 舵机 0°/180° 交替（1 秒一次）
 * @version 0.6
 * @date 2026-09-15
 */

#include "thread.hpp"
#include "Init_entry.hpp"
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include "pwm.hpp"

#pragma message "Compiling Thread/Test"

namespace thread::test {

static Thread<2048> thread_ {};
static Pwm servo_ {};

static void Task(void*, void*, void*)
{
    static constexpr uint32_t kPulse0Ns   = 500000;    // 0°   脉宽 0.5ms
    static constexpr uint32_t kPulse180Ns = 2500000;   // 180° 脉宽 2.5ms
    static constexpr uint32_t kHoldMs     = 1000;

    for (;;)
    {
        servo_.SetPulse(kPulse0Ns);
        k_msleep(kHoldMs);

        servo_.SetPulse(kPulse180Ns);
        k_msleep(kHoldMs);
    }
}

bool thread_init()
{
    return servo_.init(PWM_DT_SPEC_GET(DT_NODELABEL(servo_pwm)));
}

bool thread_start()
{
    thread_.Start(Task, ThreadPrio::High, nullptr, "test");
    return true;
}

REGISTER_INIT  (thread_init,  LateInit, High, HaltOnFail, "test_init");
REGISTER_THREAD(thread_start, LateThread, "test_start");

} // namespace thread::test
