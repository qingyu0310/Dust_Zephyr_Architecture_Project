/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 测试线程 — USB 收发性能测试
 * @version 0.2
 * @date 2026-07-27
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "thread.hpp"
#include "Init_entry.hpp"
#include "usb.hpp"
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#pragma message "Compiling Thread/Test"

LOG_MODULE_REGISTER(test, LOG_LEVEL_INF);

namespace thread::test {

static usb::Usb usb_ {};
static Thread<2048> thread_ {};

static void Task(void*, void*, void*)
{
    constexpr uint8_t tick[]  = "tick\r\n";

    for (;;)
    {
        usb_.Send(tick, sizeof(tick) - 1);
        k_msleep(1000);
    }
}

bool thread_init()
{
    UsbHal::Config cfg {};
    cfg.reg_base    = DT_REG_ADDR(DT_NODELABEL(qingyuusb_usb0));
    cfg.irq_num     = DT_IRQN(DT_NODELABEL(qingyuusb_usb0));

    while (!usb_.Init(cfg)) {
        k_msleep(100);
    }

    return true;
}

bool thread_start()
{
    thread_.Start(Task, ThreadPrio::High);
    return true;
}

REGISTER_INIT(thread_init,  PreInit,    High, "test_init");
REGISTER_INIT(thread_start, LateThread, High, "test_start");

} // namespace thread::test
