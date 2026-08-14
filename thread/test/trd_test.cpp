/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 测试线程 —— USB CDC ACM 回环（收到即回传）
 * @version 0.4
 * @date 2026-08-14
 */

#include "thread.hpp"
#include "Init_entry.hpp"
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include "usb.hpp"

#pragma message "Compiling Thread/Test"

namespace thread::test {

static Thread<2048> thread_ {};
static usb::Usb usb_ {};

static void Task(void*, void*, void*)
{
    static uint8_t rx_buf[usb::Usb::kRxBufSize];

    for (;;)
    {
        // 阻塞等数据，收到即回传（USB CDC ACM 回环）
        k_sem_take(&usb_.sem_, K_FOREVER);
        uint16_t n = usb_.Read(rx_buf, sizeof(rx_buf));
        if (n > 0) {
            usb_.Send(rx_buf, n);
        }
    }
}

bool thread_init()
{
    UsbHal::Config cfg {};

    cfg.reg_base     = DT_REG_ADDR(DT_NODELABEL(dustusb_usb0));
    cfg.irq_num      = DT_IRQN(DT_NODELABEL(dustusb_usb0));
    cfg.irq_priority = 5;

    while (!usb_.Init(cfg)) {
        k_msleep(100);
    }
    return true;
}

bool thread_start()
{
    if (!usb_.IsReady()) {
        return false;
    }

    thread_.Start(Task, ThreadPrio::High, nullptr, "test");
    return true;
}

REGISTER_INIT  (thread_init,  LateInit, High, HaltOnFail, "test_init");
REGISTER_THREAD(thread_start, LateThread, "test_start");

} // namespace thread::test
