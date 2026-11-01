/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 测试线程 —— EtherCAT 从站
 * @version 4.3
 * @date 2026-10-31
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <cstring>

#include "Init_entry.hpp"
#include "ecat_hal_hpm.hpp"
#include "ecat_phy.hpp"
#include "ethercat.hpp"
#include "jl1111/phy_jl1111.hpp"
#include "log.hpp"
#include "shell_cmd.hpp"
#include "token.hpp"
#include "spec/ecat_pdo_layout.hpp"
#include "thread.hpp"

#pragma message "Compiling Thread/Test"

namespace thread::test {

using ecat::PdoInputs;
using ecat::PdoOutputs;

static Thread<4096> thread_ {};
static ecat::EthercatSlave slave_ {};

static atomic_t s_rx_callbacks = ATOMIC_INIT(0);	// 接收回调累计次数，包含长度异常帧
static uint32_t s_reported_rx_callbacks = 0;		// 上次报告时的回调计数
static bool s_rx_inactive_reported = false;		// 停收只报告一次，恢复后重新允许报告
static atomic_t s_last_rx_ms = ATOMIC_INIT(0);		// 最近一次进入接收回调的时间
static atomic_t s_last_rx_length = ATOMIC_INIT(0);	// 最近一次回调的数据长度
static atomic_t s_rx_bad_length = ATOMIC_INIT(0);	// 长度不符的回调累计次数
static int64_t s_next_rx_report = 0;				// 线程每秒输出一次回调计数

static uint32_t s_input_counter = 0;	// 从站的输入计数器，每帧递增

/**
 * @brief 主站数据到达（PDI ISR 上下文）
 *
 * 只做拷贝与打点，不阻塞、不分配。
 * 日志分两条：DUST_LOG 队列帧只有 4 个整数槽，超过会丢字段。
 * DBG 流 ecat_rx 供 shell `log on ecat_rx` 按需观察。
 *
 * @param context 未使用
 * @param data    主站发来的字节
 * @param length  字节数
 */
static void on_master_data(void* context, const uint8_t* data, uint16_t length)
{
	(void)context;
	atomic_inc(&s_rx_callbacks);
	atomic_set(&s_last_rx_ms, static_cast<atomic_val_t>(k_uptime_get_32()));
	atomic_set(&s_last_rx_length, static_cast<atomic_val_t>(length));

	if (length != sizeof(PdoOutputs)) {
		atomic_inc(&s_rx_bad_length);
		DUST_LOG_WRN("ecat rx %u bytes, expect %u", static_cast<unsigned>(length), static_cast<unsigned>(sizeof(PdoOutputs)));
		return;
	}

	PdoOutputs value {};

	std::memcpy(&value, data, sizeof(value));

	// DUST_LOG_INF("ecat rx[0..3]: %u %u %u %u", value.counter[0], value.counter[1], value.counter[2], value.counter[3]);
	// DUST_LOG_INF("ecat rx[4..7]: %u %u %u %u", value.counter[4], value.counter[5], value.counter[6], value.counter[7]);

	DUST_LOG_DBG("ecat_rx", "rx: %u %u %u %u", value.counter[0], value.counter[1], value.counter[2], value.counter[3]);
}

/**
 * @brief 填充待发数据（PDI ISR 上下文）
 * @param context 未使用
 * @param data    目标缓冲
 * @param length  字节数
 */
static void fill_inputs(void* context, uint8_t* data, uint16_t length)
{
	(void)context;

	if (length != sizeof(PdoInputs)) {
		DUST_LOG_WRN("ecat tx %u bytes, expect %u", static_cast<unsigned>(length), static_cast<unsigned>(sizeof(PdoInputs)));
		return;
	}

	PdoInputs value {};

	for (uint8_t i = 0; i < ecat::kProcessInputWords; ++i) {
		value.counter[i] = s_input_counter;
	}

	++s_input_counter;

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

	const int64_t now = k_uptime_get();

	if (now >= s_next_rx_report)
	{
		s_next_rx_report = now + 1000;
		const unsigned key = irq_lock();
		const uint32_t callbacks = static_cast<uint32_t>(atomic_get(&s_rx_callbacks));
		const uint32_t age = (callbacks != 0) ? k_uptime_get_32() - static_cast<uint32_t>(atomic_get(&s_last_rx_ms)) : 0;
		const uint32_t length = static_cast<uint32_t>(atomic_get(&s_last_rx_length));
		const uint32_t bad_length = static_cast<uint32_t>(atomic_get(&s_rx_bad_length));
		const ::AlState state = slave_.State();

		irq_unlock(key);

		if (state == ::AlState::Op && callbacks != s_reported_rx_callbacks) {
			s_rx_inactive_reported = false;
			DUST_LOG_INF("ecat app: t=%u rx_callbacks=%u", static_cast<unsigned>(k_uptime_get_32()), static_cast<unsigned>(callbacks));
		} else if ((callbacks != 0 || state == ::AlState::Op) && !s_rx_inactive_reported) {
			DUST_LOG_ERR("ecat app: t=%u state=0x%02x callbacks=%u age_ms=%u", static_cast<unsigned>(k_uptime_get_32()), static_cast<unsigned>(state), static_cast<unsigned>(callbacks), static_cast<unsigned>(age));
			DUST_LOG_ERR("ecat app rx detail: len=%u expected=%u bad_length=%u", static_cast<unsigned>(length), static_cast<unsigned>(sizeof(PdoOutputs)), static_cast<unsigned>(bad_length));
			s_rx_inactive_reported = true;
		}

		s_reported_rx_callbacks = callbacks;
	}

	return TaskResult::Continue;
}

/**
 * @brief 主动读取或清除保存的首次 EtherCAT 故障，停收时不需要翻串口历史
 * @param args fault 或 fault clear
 */
static void EcatCommand(uint8_t* args)
{
	const char* command = nullptr;
	uint8_t* rest = shell::NextToken(args, command);

	if (std::strcmp(command, "fault") != 0) {
		shell::Log::SendCommandLine("usage: ecat fault [clear]");
		return;
	}

	if (*rest == '\0') 
	{
		shell::Log::SendCommandLine(slave_.RequestFaultReport() ? "ecat: saved fault report requested" : "ecat: no saved fault");
	} 
	else if (std::strcmp(reinterpret_cast<const char*>(rest), "clear") == 0) 
	{
		slave_.ClearFaultReport();
		shell::Log::SendCommandLine("ecat: fault clear requested");
	} 
	else 
	{
		shell::Log::SendCommandLine("usage: ecat fault [clear]");
	}
}

REGISTER_SHELL_CMD("ecat", "ecat fault [clear]", EcatCommand);

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

	slave_.SetOutputSink(on_master_data, nullptr);
	slave_.SetInputSource(fill_inputs,  nullptr);

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

}
