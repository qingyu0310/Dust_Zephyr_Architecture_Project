/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 测试线程 —— EtherCAT 从站（SSC 5.13）
 * @version 3.3
 * @date 2026-10-30
 */

#include "log.hpp"
#include "thread.hpp"
#include "Init_entry.hpp"
#include <zephyr/kernel.h>
#include <cstring>
#include "ethercat.hpp"

#pragma message "Compiling Thread/Test"

namespace thread::test {

static Thread<4096> thread_ {};
static ecat::EthercatSlave slave_;

/**
 * @brief 主站写给从站的过程数据（RxPDO），8 个 UDINT，共 32 字节
 */
struct PdoOutputs
{
    uint32_t counter[8];    // 输出计数器
};

/**
 * @brief 从站回给主站的过程数据（TxPDO），8 个 UDINT，共 32 字节
 */
struct PdoInputs
{
    uint32_t counter[8];    // 输入计数器
};

static_assert(sizeof(PdoOutputs) == 32);
static_assert(sizeof(PdoInputs) == 32);

/**
 * @brief 任务单拍：推一拍协议栈，立刻返回
 * @param context 未使用
 * @return Continue=正常
 */
static TaskResult Task(void* context)
{
    (void)context;

    slave_.Poll();

    return TaskResult::Continue;
}

bool thread_init()
{
    if (!slave_.Init()) {
        return false;
    }

    // 主站 → 从站（SM2 / RxPDO）：8 个 uint32_t，共 32 字节
    slave_.SetRxHandler([](const uint8_t* data, size_t len)
	{
        if (len != sizeof(PdoOutputs)) {
            DUST_LOG_WRN("ecat rx %u bytes, expect %u",
                         static_cast<unsigned>(len),
                         static_cast<unsigned>(sizeof(PdoOutputs)));
            return;
        }

        PdoOutputs value {};

        memcpy(&value, data, sizeof(value));
        DUST_LOG_INF("ecat rx[0..3]: %u %u %u %u",
                     value.counter[0], value.counter[1], value.counter[2], value.counter[3]);
        DUST_LOG_INF("ecat rx[4..7]: %u %u %u %u",
                     value.counter[4], value.counter[5], value.counter[6], value.counter[7]);
    });

    // 从站 → 主站（SM3 / TxPDO）
    slave_.SetTxProvider([](uint8_t* data, size_t len)
	{
        if (len != sizeof(PdoInputs)) {
            DUST_LOG_WRN("ecat tx %u bytes, expect %u",
                         static_cast<unsigned>(len),
                         static_cast<unsigned>(sizeof(PdoInputs)));
            return;
        }

        PdoInputs value {};
        for (size_t i = 0; i < 8; i++) {
            value.counter[i] = static_cast<uint32_t>(i);
        }
        memcpy(data, &value, sizeof(value));
    });

    return true;
}

bool thread_start()
{
    // 1ms 一拍：EtherCAT 协议栈主循环 + 链路轮询（内部 100ms 才真正刷新）
    thread_.Start(Task, K_MSEC(1), ThreadPrio::High, nullptr, "test");
    return true;
}

REGISTER_INIT(thread_init, LateInit, High, HaltOnFail, nullptr, "test_init");
REGISTER_THREAD(thread_start, LateThread, ContinueOnFail, "test_start");

} // namespace thread::test
