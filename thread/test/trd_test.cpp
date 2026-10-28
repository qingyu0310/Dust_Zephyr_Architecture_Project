/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 测试线程 —— PD8 / PD9 GPIO 翻转（验引脚对应关系）
 * @version 2.1
 * @date 2026-10-28
 */

#include "log.hpp"
#include "thread.hpp"
#include "Init_entry.hpp"
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include "output.hpp"

#pragma message "Compiling Thread/Test"
 
namespace thread::test {

static constexpr uint32_t kPeriodMs = 500;                      // 翻转周期

static Thread<2048> thread_ {};
static Output       pd14_    {};
static Output       pd15_    {};

/**
 * @brief 任务单拍：翻转 PD8 / PD9
 * @param context 未使用
 * @return Continue=继续下一拍
 */
static TaskResult Task(void* context)
{
    (void)context;

    pd14_.Toggle();
    pd15_.Toggle();

	DUST_LOG_INF("run in");

    return TaskResult::Continue;
}

bool thread_init()
{
    static const gpio_dt_spec pd14_spec = {
        .port     = DEVICE_DT_GET(DT_NODELABEL(gpiod)),
        .pin      = 14,
        .dt_flags = 0,              // 方向由 Output::init(默认 GPIO_OUTPUT) 配置
    };
    static const gpio_dt_spec pd15_spec = {
        .port     = DEVICE_DT_GET(DT_NODELABEL(gpiod)),
        .pin      = 15,
        .dt_flags = 0,              // 方向由 Output::init(默认 GPIO_OUTPUT) 配置
    };

    if (!pd14_.init(pd14_spec) || !pd15_.init(pd15_spec)) {
        return false;
    }

    return true;
}

bool thread_start()
{
    thread_.Start(Task, K_MSEC(kPeriodMs), ThreadPrio::High, nullptr, "test");
    return true;
}

REGISTER_INIT(thread_init, LateInit, High, HaltOnFail, nullptr, "test_init");
REGISTER_THREAD(thread_start, LateThread, ContinueOnFail, "test_start");

} // namespace thread::test
