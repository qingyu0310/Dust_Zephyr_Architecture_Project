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

#pragma message "Compiling Thread/Test"

namespace thread::test {

static Thread<2048> thread_ {};

static void Task(void*, void*, void*)
{
    for (;;)
    {
        k_msleep(1);
    }
}

bool thread_init()
{
    return true;
}

bool thread_start()
{
    thread_.Start(Task, ThreadPrio::High, nullptr, "test");
    return true;
}

REGISTER_INIT  (thread_init,  LateInit, High, HaltOnFail, "test_init");
REGISTER_THREAD(thread_start, LateThread, "test_start");

} // namespace thread::test
