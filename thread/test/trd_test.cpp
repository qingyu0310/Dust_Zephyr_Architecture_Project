/**
 * @file trd_test.cpp
 * @brief 原始 UART 帧抓取 — 25 字节 SBUS 帧对齐打印
 */

#include "thread.hpp"
#include "uart.hpp"
#include "Init_entry.hpp"
#include "zephyr/kernel.h"
#include <zephyr/logging/log.h>

#pragma message "Compiling Thread/Test"

LOG_MODULE_REGISTER(test, LOG_LEVEL_INF);

namespace thread::test {

static constexpr uint16_t kFrameSize = 25;

static Thread<4096> thread_ {};
static UartDma      uart_ {};
static uint8_t      buf_[kFrameSize];
static uint16_t     pos_;

static void Task(void*, void*, void*)
{
    for (;;)
    {
        uint16_t n = uart_.Read(buf_ + pos_, kFrameSize - pos_);
        if (n == 0) {
            k_msleep(10);
            continue;
        }
        pos_ += n;

        if (pos_ >= kFrameSize)
        {
            if (buf_[0] != 0x0F) goto skip;
            uint16_t c[16];
            c[0]  = (buf_[1]      | buf_[2]  << 8) & 0x07FF;
            c[1]  = (buf_[2] >> 3 | buf_[3]  << 5) & 0x07FF;
            c[2]  = (buf_[3] >> 6 | buf_[4]  << 2 | buf_[5]  << 10) & 0x07FF;
            c[3]  = (buf_[5] >> 1 | buf_[6]  << 7) & 0x07FF;
            c[4]  = (buf_[6] >> 4 | buf_[7]  << 4) & 0x07FF;
            c[5]  = (buf_[7] >> 7 | buf_[8]  << 1 | buf_[9]  << 9) & 0x07FF;
            c[6]  = (buf_[9] >> 2 | buf_[10] << 6) & 0x07FF;
            c[7]  = (buf_[10] >> 5 | buf_[11] << 3) & 0x07FF;
            c[8]  = (buf_[12]     | buf_[13] << 8) & 0x07FF;
            c[9]  = (buf_[13] >> 3| buf_[14] << 5) & 0x07FF;
            c[10] = (buf_[14] >> 6| buf_[15] << 2 | buf_[16] << 10) & 0x07FF;
            c[11] = (buf_[16] >> 1| buf_[17] << 7) & 0x07FF;
            c[12] = (buf_[17] >> 4| buf_[18] << 4) & 0x07FF;
            c[13] = (buf_[18] >> 7| buf_[19] << 1 | buf_[20] << 9) & 0x07FF;
            c[14] = (buf_[20] >> 2| buf_[21] << 6) & 0x07FF;
            c[15] = (buf_[21] >> 5| buf_[22] << 3) & 0x07FF;

            printk("SBUS: %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
                   c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7],
                   c[8], c[9], c[10], c[11], c[12], c[13], c[14], c[15]);
skip:
            pos_ = 0;
        }
    }
}

bool thread_init()
{
    UartDma::Config cfg = {
        .line_cfg = {
            .baudrate    = 100000,
            .parity      = UART_CFG_PARITY_EVEN,
            .stop_bits   = UART_CFG_STOP_BITS_2,
            .data_bits   = UART_CFG_DATA_BITS_8,
            .flow_ctrl   = UART_CFG_FLOW_CTRL_NONE,
        },
        .base_cfg = { .rx_timeout = 1000 },
    };

    if (!uart_.Init(DEVICE_DT_GET(DT_ALIAS(remote_uart)), cfg))
    {
        LOG_ERR("uart init failed");
        return false;
    }

    LOG_INF("uart ready on remote_uart");
    return true;
}

bool thread_start()
{
    pos_ = 0;
    thread_.Start(Task, ThreadPrio::Lowest);
    return true;
}

REGISTER_INIT(thread_init,  Module,     Low, "test_init");
REGISTER_INIT(thread_start, ThreadLate, Low, "test_start");

} // namespace thread::test