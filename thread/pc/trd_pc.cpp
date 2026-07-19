/**
 * @file trd_pc.cpp
 * @author qingyu
 * @brief PC 通信线程 — USB CDC ACM 回环测试
 * @version 0.2
 * @date 2026-06-11
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma message "Compiling Thread/Pc"

#include "thread.hpp"
#include "Init_entry.hpp"
#include <zephyr/kernel.h>
#include "usb.hpp"
#include <zephyr/devicetree.h>

namespace thread::pc {

static Thread<2048> thread_ {};
static Usb usb_ {};

static void Task(void*, void*, void*)
{
    constexpr uint8_t send[] = "tick\r\n";
    for (;;)
    {
        usb_.Send(send, sizeof(send));
        k_sleep(K_MSEC(1000));
    }
}

bool thread_init()
{
    Usb::Config cfg {};
    cfg.busid = 0;
    cfg.reg_base = DT_REG_ADDR(DT_NODELABEL(cherryusb_usb0));
    cfg.buf_size = 512;

    while (!usb_.Init(cfg)) {
        // printf
    };
    return true;
}

bool thread_start()
{
    if (!usb_.IsReady()) {
        return false;
    }

    thread_.Start(Task, 6);
    return true;
}

REGISTER_INIT(thread_init,  Module, Low, "pc_init");
REGISTER_INIT(thread_start, Thread, Low, "pc_start");

} // namespace thread::pc
