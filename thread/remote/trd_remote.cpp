/**
 * @file trd_remote.cpp
 * @author qingyu
 * @brief 遥控器线程 — UART 接收、协议解析
 * @version 0.1
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma message "Compiling Thread/Remote"

#include "remote.hpp"
#include "Init_entry.hpp"
#include "uart.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(remote, LOG_LEVEL_INF);

namespace thread::remote {

static ::remote::Remote remote_ {};

/**
 * @brief 初始化 UART DMA，并将遥控器解码器配置为自动识别模式。
 */
bool thread_init()
{
    static UartDma rx {};
    RxStream::Config cfg {};

    constexpr uint16_t kBufferSize = 128;
    constexpr uint16_t kTimeour    = 1000;

    cfg.buf_size   = kBufferSize;
    cfg.rx_timeout = kTimeour;

    if (!rx.Init(DEVICE_DT_GET(DT_ALIAS(remote_uart)), cfg)) {
        LOG_ERR("uart init failed");
        return false;
    }

    remote_.Init(RemoteType::Auto, rx);
    return true;
}

bool thread_start()
{
    return remote_.Start(5);
}

REGISTER_INIT(thread_init,  Module, High, "remote_init");
REGISTER_INIT(thread_start, Thread, High, "remote_start");

} // namespace thread::remote
