/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 测试线程 —— VOFA+ FireWater 双通道波形输出（受管周期任务）
 * @version 0.7
 * @date 2026-10-19
 */

#include "thread.hpp"
#include "Init_entry.hpp"
#include "log.hpp"
#include <zephyr/kernel.h>
#include <cmath>

#pragma message "Compiling Thread/Test"

namespace thread::test {

static Thread<2048> thread_ {};

/**
 * @brief 任务单拍：推进仿真时间并输出一组 VOFA+ 波形
 * @param context 未使用
 * @return Continue=正常
 */
static TaskResult Task(void* context)
{
    (void)context;

    // VOFA+ FireWater 测试：log mode vofa on 后应为纯文本 "v0,v1\r\n"，VOFA+ 出两条曲线
    DUST_LOG_DBG("vofa", "%f,%f", 1.0, 2.0);

    return TaskResult::Continue;
}

bool thread_init()
{
    return true;
}

bool thread_start()
{
    thread_.Start(Task, K_MSEC(100), ThreadPrio::High, nullptr, "test");
    return true;
}

REGISTER_INIT(thread_init, LateInit, High, HaltOnFail, nullptr, "test_init");
REGISTER_THREAD(thread_start, LateThread, "test_start");

} // namespace thread::test
