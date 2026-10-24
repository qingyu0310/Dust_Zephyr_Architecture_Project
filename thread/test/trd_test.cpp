/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 测试线程 —— 线程故障处置验证（var 命令控制）
 * @version 1.5
 * @date 2026-10-24
 */

#include "thread.hpp"
#include "Init_entry.hpp"
#include "var.hpp"
#include "log.hpp"
#include <zephyr/kernel.h>

#pragma message "Compiling Thread/Test"

namespace thread::test {

static Thread<2048> thread_ {};

// 0=空闲；1=Retry（钩子扛住，继续）；2=Exit（跳过钩子，按策略处置）
static uint32_t trigger_ = 0;

REGISTER_SHELL_VAR("test_trigger", trigger_);

/**
 * @brief Retry 钩子：扛住并打日志
 * @param context 未使用
 * @return true=已扛住，线程继续下一拍
 */
static bool OnRetry(void* context)
{
    (void)context;

    DUST_LOG_INF("retry handled");

    return true;
}

/**
 * @brief 亡语：线程临死前打一条日志
 * @param context 未使用
 */
static void OnExit(void* context)
{
    (void)context;

    DUST_LOG_INF("exit hook");
}

/**
 * @brief 任务单拍：检查触发开关
 * @param context 未使用
 * @return Continue=正常；Retry=走钩子；Exit=跳过钩子按策略处置
 */
static TaskResult Task(void* context)
{
    (void)context;

    if (trigger_ == 1)
    {
        trigger_ = 0;
        return TaskResult::Retry;       // 钩子扛住：线程继续，retry_count++
    }

    if (trigger_ == 2)
    {
        trigger_ = 0;
        return TaskResult::Exit;        // 跳过钩子 → DefaultHandle → 亡语 → 终止
    }

    return TaskResult::Continue;
}

bool thread_init()
{
    return true;
}

bool thread_start()
{
    thread_.Start(Task, K_MSEC(10), ThreadPrio::High, nullptr, "test",
                  ThreadFaultConfig{ OnRetry, OnExit, ThreadFaultPolicy::Recover });
    return true;
}

REGISTER_INIT(thread_init, LateInit, High, HaltOnFail, nullptr, "test_init");
REGISTER_THREAD(thread_start, LateThread, ContinueOnFail, "test_start");

} // namespace thread::test
