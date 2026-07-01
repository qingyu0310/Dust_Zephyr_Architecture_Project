/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief UART 接收性能基准测试
 * @version 0.1
 * @date 2026-07-01
 */

#pragma message "Compiling Thread/Test"

#include "trd_test.hpp"
#include "thread.hpp"
#include "uart.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(test, LOG_LEVEL_INF);

namespace thread::test {

static Thread<4096> thread_ {};

static UartDma uart3 {};
static k_sem   uart3_sem {};

static struct {
    uint32_t total_bytes;       // 总接收字节数
    uint32_t read_count;        // Read() 调用次数
    uint32_t min_per_read;      // 单次 Read 最小字节
    uint32_t max_per_read;      // 单次 Read 最大字节
    uint32_t read_cycles;       // Read() 累计 CPU 周期数
    int64_t  start_ms;          // 测试起始时间戳
    bool     started;           // 是否已开始接收
} g_stats = {};

static void reset_stats()
{
    g_stats = {};
    g_stats.min_per_read = UINT32_MAX;
}

static void report_stats()
{
    if (g_stats.read_count == 0) return;

    int64_t elapsed_ms = k_uptime_delta(&g_stats.start_ms);
    if (elapsed_ms <= 0) elapsed_ms = 1;

    uint32_t avg_per_read = g_stats.total_bytes / g_stats.read_count;
    uint32_t avg_read_cyc = g_stats.read_cycles / g_stats.read_count;

    // 通过串口打印报告，Python 脚本通过 [BENCH] 标记解析
    printk("[BENCH] bytes=%u reads=%u min=%u max=%u avg=%u "
           "rd_cyc=%u avg_cyc=%u time=%lldms bw=%uB/s\n",
           g_stats.total_bytes,
           g_stats.read_count,
           g_stats.min_per_read,
           g_stats.max_per_read,
           avg_per_read,
           g_stats.read_cycles,
           avg_read_cyc,
           (long long)elapsed_ms,
           (uint32_t)(g_stats.total_bytes * 1000 / elapsed_ms));
}

static void Task(void*, void*, void*)
{
    for (;;)
    {
        int ret = k_sem_take(&uart3_sem, K_MSEC(200));

        if (ret == 0)
        {
            uint8_t buf[256];
            uint32_t t0 = k_cycle_get_32();
            uint16_t len = uart3.Read(buf, sizeof(buf));
            uint32_t t1 = k_cycle_get_32();

            if (len > 0)
            {
                if (!g_stats.started) {
                    g_stats.started = true;
                    g_stats.start_ms = k_uptime_get();
                }

                g_stats.total_bytes  += len;
                g_stats.read_count++;
                g_stats.read_cycles  += (t1 - t0);
                if (len < g_stats.min_per_read) g_stats.min_per_read = len;
                if (len > g_stats.max_per_read) g_stats.max_per_read = len;
            }
        }
        else
        {
            // 200ms 无数据 → 上报当前测试结果
            if (g_stats.started) {
                report_stats();
                reset_stats();
            }
        }
    }
}

void thread_init()
{
    k_sem_init(&uart3_sem, 0, 1);

    RxStream::Config cfg {};
    cfg.buf_size   = 256;
    cfg.rx_timeout = 1000;

    if (!uart3.Init(DEVICE_DT_GET(DT_NODELABEL(uart3)), cfg)) {
        LOG_ERR("uart3 init failed");
        return;
    }

    uart3.SetNotify(&uart3_sem);
    LOG_INF("uart3 ready");
}

void thread_start(uint8_t prio)
{
    thread_.Start(Task, prio);
}

} // namespace thread::test
