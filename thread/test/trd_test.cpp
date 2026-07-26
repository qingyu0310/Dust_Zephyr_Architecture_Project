/**
 * @file trd_test.cpp
 * @brief Buzzer 宏测试
 */

#include "thread.hpp"
#include "Init_entry.hpp"
#include <zephyr/logging/log.h>

#pragma message "Compiling Thread/Test"

LOG_MODULE_REGISTER(test, LOG_LEVEL_INF);

namespace thread::test {

static Thread<1024> thread_ {};

static void Task(void*, void*, void*)
{
    for (;;)
    {
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

} // namespace thread::test
