/**
 * @file trd_test.cpp
 * @brief Shell 调试变量测试 — 注册各类型变量并定时更新
 */

#include "thread.hpp"
#include "shell.hpp"
#include "Init_entry.hpp"
#include <zephyr/logging/log.h>

#pragma message "Compiling Thread/Test"

LOG_MODULE_REGISTER(test, LOG_LEVEL_INF);

namespace thread::test {

/** @brief 测试用简单类 — 验证 REGISTER_SHELL_VAR 支持类成员 */
struct TestObj
{
    int16_t  value;
    bool     flag;
    float    fader;
};

static uint32_t loop_count;
static float    test_value;
static bool     test_flag;
static int16_t  test_signal;

static TestObj   test_obj { .value = 42, .flag = true, .fader = 0.5f };
static Thread<1024> thread_ {};

static void Task(void*, void*, void*)
{
    for (;;)
    {
        // test_obj.value++;
        // test_obj.fader += 0.1f;
        // if (test_obj.fader > 1.0f) test_obj.fader = 0.0f;
        // test_obj.flag = !test_obj.flag;

        k_msleep(1000);
    }
}

bool thread_init()
{
    return true;
}

bool thread_start()
{
    thread_.Start(Task, ThreadPrio::Lowest);
    return true;
}

REGISTER_INIT(thread_init,  AppInit,    Low, "test_init");
REGISTER_INIT(thread_start, AppThread,  Low, "test_start");

REGISTER_SHELL_VAR("test_loop",    loop_count);
REGISTER_SHELL_VAR("test_value",   test_value);
REGISTER_SHELL_VAR("test_flag",    test_flag);
REGISTER_SHELL_VAR("test_signal",  test_signal);
REGISTER_SHELL_VAR("obj_value",    test_obj.value);
REGISTER_SHELL_VAR("obj_flag",     test_obj.flag);
REGISTER_SHELL_VAR("obj_fader",    test_obj.fader);

} // namespace thread::test
