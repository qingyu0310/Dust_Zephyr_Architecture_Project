/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 测试线程 —— EtherCAT 从站
 * @version 4.2
 * @date 2026-10-31
 */

#include <zephyr/kernel.h>

#include <cstring>

#include "Init_entry.hpp"
#include "ecat_hal_hpm.hpp"
#include "ecat_phy.hpp"
#include "ethercat.hpp"
#include "jl1111/phy_jl1111.hpp"
#include "log.hpp"
#include "thread.hpp"

#pragma message "Compiling Thread/Test"

namespace thread::test {

static Thread<4096> thread_ {};
static ::ecat::EthercatSlave slave_ {};

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

static_assert(sizeof(PdoOutputs) == 32, "RxPDO 必须是 32 字节");
static_assert(sizeof(PdoInputs)  == 32, "TxPDO 必须是 32 字节");

/** 从站的输入计数器，每帧递增 */
static uint32_t s_input_counter = 0;

/**
 * @brief 主站数据到达（PDI ISR 上下文）
 *
 * 只做拷贝与打点，不阻塞、不分配。
 * 日志分两条：DUST_LOG 队列帧只有 4 个整数槽，超过会丢字段。
 *
 * @param context 未使用
 * @param data    主站发来的字节
 * @param length  字节数
 */
static void OnMasterData(void* context, const uint8_t* data, uint16_t length)
{
    (void)context;

    if (length != sizeof(PdoOutputs)) {
        DUST_LOG_WRN("ecat rx %u bytes, expect %u", static_cast<unsigned>(length), static_cast<unsigned>(sizeof(PdoOutputs)));
        return;
    }

    PdoOutputs value {};

    std::memcpy(&value, data, sizeof(value));
    DUST_LOG_INF("ecat rx[0..3]: %u %u %u %u", value.counter[0], value.counter[1], value.counter[2], value.counter[3]);
    DUST_LOG_INF("ecat rx[4..7]: %u %u %u %u", value.counter[4], value.counter[5], value.counter[6], value.counter[7]);
}

/**
 * @brief 填充待发数据（PDI ISR 上下文）
 * @param context 未使用
 * @param data    目标缓冲
 * @param length  字节数
 */
static void FillInputs(void* context, uint8_t* data, uint16_t length)
{
    (void)context;

    if (length != sizeof(PdoInputs)) {
        DUST_LOG_WRN("ecat tx %u bytes, expect %u", static_cast<unsigned>(length), static_cast<unsigned>(sizeof(PdoInputs)));
        return;
    }

    PdoInputs value {};

    for (size_t i = 0; i < 8; i++) {
        value.counter[i] = s_input_counter;
    }
    s_input_counter++;

    std::memcpy(data, &value, sizeof(value));
}

/**
 * @brief 任务单拍：推一拍协议栈
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
    static ::ecat::EcatEscHpm esc {};
    static ::PhyJl1111 phy0 {};
    static ::PhyJl1111 phy1 {};

    static ::EcatPhy* const phys[] = { &phy0, &phy1 };

    if (!slave_.Init(esc, phys)) {
        DUST_LOG_ERR("ecat: init failed");
        return false;
    }

    slave_.SetOutputSink(OnMasterData, nullptr);
    slave_.SetInputSource(FillInputs, nullptr);

    DUST_LOG_INF("ecat: ready rx=%u tx=%u", static_cast<unsigned>(slave_.Layout().rx_size), static_cast<unsigned>(slave_.Layout().tx_size));
    return true;
}

bool thread_start()
{
    // 1ms 一拍：AL 状态、邮箱收发、链路轮询都在协议栈内部推进
    thread_.Start(Task, K_MSEC(1), ThreadPrio::High, nullptr, "test");
    return true;
}

REGISTER_INIT(thread_init, LateInit, High, HaltOnFail, nullptr, "test_init");
REGISTER_THREAD(thread_start, LateThread, ContinueOnFail, "test_start");

} // namespace thread::test
