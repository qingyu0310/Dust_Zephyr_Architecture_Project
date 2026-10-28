/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 测试线程 —— EtherCAT 从站（SSC 5.13）
 * @version 3.0
 * @date 2026-10-28
 */

#include "log.hpp"
#include "thread.hpp"
#include "Init_entry.hpp"
#include <zephyr/kernel.h>
#include "ethercat.hpp"

#pragma message "Compiling Thread/Test"

namespace thread::test {

static Thread<4096> thread_ {};

/**
 * @brief 任务单拍：跑一轮协议栈主循环
 * @param context 未使用
 * @return Continue=正常
 */
static TaskResult Task(void* context)
{
    (void)context;

    EthercatSlave::Instance().Run();

    return TaskResult::Continue;
}

bool thread_init()
{
    EthercatSlave& ecat = EthercatSlave::Instance();

    if (!ecat.Init()) {
        return false;
    }

    // 主站 → 从站（SM2 / RxPDO）
    ecat.SetRxHandler([](const uint8_t* data, size_t len) 
	{
        DUST_LOG_INF("ecat rx %u bytes", static_cast<unsigned>(len));
        (void)data;
    });

    // 从站 → 主站（SM3 / TxPDO）
    ecat.SetTxProvider([](uint8_t* data, size_t len)
	{
        for (size_t i = 0; i < len; i++) {
            data[i] = static_cast<uint8_t>(i);
        }
    });

    return true;
}

bool thread_start()
{
    thread_.Start(Task, K_NO_WAIT, ThreadPrio::High, nullptr, "test");
    return true;
}

REGISTER_INIT(thread_init, LateInit, High, HaltOnFail, nullptr, "test_init");
REGISTER_THREAD(thread_start, LateThread, ContinueOnFail, "test_start");

} // namespace thread::test
