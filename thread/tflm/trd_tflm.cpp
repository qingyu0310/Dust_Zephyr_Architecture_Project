/**
 * @file trd_tflm.cpp
 * @author qingyu
 * @brief TFLM Hello World inference thread
 * @version 0.1
 * @date 2026-05-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "thread.hpp"
#include "Init_entry.hpp"
#include "tflm.hpp"
#include "zephyr/kernel.h"

namespace thread::ml {

static Thread<8192> thread_{};

static void Task(void*, void*, void*)
{
    for (;;)
    {
        k_msleep(1000);
    }
}

bool thread_init()
{
    tflm::init();
    tflm::print_info();
    return true;
}

bool thread_start()
{
    thread_.Start(Task, 10);
    return true;
}

REGISTER_INIT(thread_init,  Module, Low, "tflm_init");
REGISTER_INIT(thread_start, Thread, Low, "tflm_start");

} // namespace thread::ml
