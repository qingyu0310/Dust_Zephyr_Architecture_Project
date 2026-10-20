/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 测试线程 —— fatal 异常触发验证（var 命令控制）
 * @version 1.0
 * @date 2026-10-20
 */

#include "thread.hpp"
#include "Init_entry.hpp"
#include "var.hpp"
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/devicetree.h>

#pragma message "Compiling Thread/Test"

namespace thread::test {

static Thread<2048> thread_ {};

// 0=空闲；1=非法地址写入；2=栈溢出；3=除零（整数）
static uint32_t trigger_ = 0;

REGISTER_SHELL_VAR("test_trigger", trigger_);

/**
 * @brief 裸 UART 打点（不依赖日志队列与 shell 线程）
 * @param dev  shell-uart 设备
 * @param text 以 '\0' 结尾的字符串
 */
static void Trace(const struct device* dev, const char* text)
{
    for (const char* p = text; *p != '\0'; ++p) {
        uart_poll_out(dev, static_cast<unsigned char>(*p));
    }
}

/**
 * @brief 触发指定类型的异常
 * @param kind 异常类型（1=非法地址 2=栈溢出 3=除零）
 */
static void TriggerFault(uint32_t kind)
{
    switch (kind)
    {
        case 1:
        {
            // HPM5361 的 0x0 是合法 ILM（ILM_LOCAL_BASE=0x0, 128KB），
            // 写它只会破坏向量表而不触发异常。用无映射地址。
            volatile uint32_t* p = reinterpret_cast<volatile uint32_t*>(0x90000000);
            *p = 0xDEADBEEF;
            break;
        }

        case 2:
        {
            volatile uint8_t big[4096] {};
            big[0] = 1;
            big[4000] = 2;
            (void)big;
            break;
        }

        case 3:
        {
            volatile int32_t zero = 0;
            volatile int32_t result = 100 / zero;
            (void)result;
            break;
        }

        default:
            break;
    }
}

/**
 * @brief 任务单拍：检查触发开关
 * @param context 未使用
 * @return Continue=正常
 */
static TaskResult Task(void* context)
{
    (void)context;

    if (trigger_ != 0) {
        const uint32_t kind = trigger_;
        trigger_ = 0;

        const struct device* dev = DEVICE_DT_GET(DT_ALIAS(shell_uart));

        Trace(dev, "\r\n[test] firing\r\n");

        TriggerFault(kind);
    }

    return TaskResult::Continue;
}

bool thread_init()
{
    return true;
}

bool thread_start()
{
    thread_.Start(Task, K_MSEC(10), ThreadPrio::High, nullptr, "test");
    return true;
}

REGISTER_INIT(thread_init, LateInit, High, HaltOnFail, nullptr, "test_init");
REGISTER_THREAD(thread_start, LateThread, ContinueOnFail, "test_start");

} // namespace thread::test
