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
#include <string.h>

#pragma message "Compiling Thread/Test"

LOG_MODULE_REGISTER(test, LOG_LEVEL_INF);

namespace thread::test {

static usb::Usb usb_ {};
static Thread<2048> thread_ {};

static void Task(void*, void*, void*)
{
    constexpr uint8_t start[] = "start\r\n";
    uint8_t txbuf[512];
    uint8_t rx_buf[512];
    bool tx_bench = false;
    for (int i = 0; i < 512; i++) txbuf[i] = (uint8_t)i;

    usb_.Send(start, sizeof(start) - 1);

    for (;;)
    {
        if (!tx_bench) {
            k_sem_take(&usb_.sem_, K_FOREVER);
        }

        if (tx_bench) {
            for (int i = 0; i < 50; i++) {
                usb_.Send(txbuf, sizeof(txbuf));
            }
            uint16_t n = usb_.Read(rx_buf, sizeof(rx_buf));
            if (n >= 4 && memcmp(rx_buf, "stop", 4) == 0) {
                usb_.Send((uint8_t*)"[BENCH] tx_done\r\n", 17);
                tx_bench = false;
            }
            continue;
        }

        uint16_t n = usb_.Read(rx_buf, sizeof(rx_buf));
        if (n == 0) continue;

        if (n >= 9 && memcmp(rx_buf, "bench_tx", 8) == 0) {
            tx_bench = true;
            usb_.Send(rx_buf, n);
            continue;
        }

        usb_.Send(rx_buf, n);
    }
}

bool thread_init()
{
    usb::Usb::Config cfg {};
    cfg.busid       = 0;
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
