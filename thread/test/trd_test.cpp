/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 测试线程 —— PWM 驱动 SG60 舵机 0°/180° 交替（1 秒一次）
 * @version 0.6
 * @date 2026-09-15
 */

#include "thread.hpp"
#include "Init_entry.hpp"
#include "log.hpp"
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <cmath>

#pragma message "Compiling Thread/Test"

namespace thread::test {

static Thread<2048> thread_ {};

static void Task(void*, void*, void*)
{
    float t = 0.0f;
    for (;;)
    {
        k_msleep(10);

        t += 0.01f;
        float ch0 = std::sinh(t);            // 通道0：正弦 [-1, 1]
        float ch1 = std::cosh(t) * 2.0f;     // 通道1：余弦 [-2, 2]

        // VOFA+ FireWater 测试：log on vofa 后应为纯文本 "v0,v1\r\n"，VOFA+ 出两条曲线
        DUST_LOG_DBG("vofa", "%f,%f", static_cast<double>(ch0), static_cast<double>(ch1));
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
