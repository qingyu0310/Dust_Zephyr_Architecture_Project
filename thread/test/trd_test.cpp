/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 测试线程 —— EtherCAT 从站
 * @version 5.1
 * @date 2026-11-07
 */

#include <zephyr/kernel.h>

#include <cstring>

#include "Init_entry.hpp"
#include "ecat_hal_hpm.hpp"
#include "ethercat.hpp"
#include "jl1111/phy_jl1111.hpp"
#include "log.hpp"
#include "shell_cmd.hpp"
#include "token.hpp"
#include "thread.hpp"

#pragma message "Compiling Thread/Test"

namespace thread::test {

/**
 * @brief 主站写给从站的过程数据（RxPDO）
 */
struct PdoOutputs
{
	uint32_t counter[64];	// 输出计数器
};

/**
 * @brief 从站回给主站的过程数据（TxPDO）
 */
struct PdoInputs
{
	uint32_t counter[64];	// 输入计数器
};

using Slave = ecat::EthercatSlave<PdoOutputs, PdoInputs>;

static_assert(sizeof(PdoOutputs) == Slave::Profile::kRxBytes, "RxPDO 字节数与 Profile 不一致");
static_assert(sizeof(PdoInputs)  == Slave::Profile::kTxBytes, "TxPDO 字节数与 Profile 不一致");

static Thread<4096> thread_ {};

static Slave slave_ {};

static ecat::EcatEscHpm esc_ {};

static uint32_t s_input_counter = 0;		// 从站的输入计数器，每帧递增

/**
 * @brief 任务单拍：写入待发数据、推一拍协议栈
 * @param context 未使用
 * @return Continue=正常
 */
static TaskResult Task(void* context)
{
	(void)context;

	PdoInputs inputs {};

	for (uint8_t i = 0; i < (sizeof(PdoInputs) / sizeof(uint32_t)); ++i) {
		inputs.counter[i] = s_input_counter;
	}

	slave_.SetInputs(inputs);
	slave_.Poll();

	++s_input_counter;

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
		DUST_LOG_RAW("usage: ecat fault [clear]");
		return;
	}

	if (*rest == '\0')
	{
		DUST_LOG_RAW("%s", slave_.RequestFaultReport() ? "ecat: saved fault report requested" : "ecat: no saved fault");
	}
	else if (std::strcmp(reinterpret_cast<const char*>(rest), "clear") == 0)
	{
		slave_.ClearFaultReport();
		DUST_LOG_RAW("ecat: fault clear requested");
	}
	else
	{
		DUST_LOG_RAW("usage: ecat fault [clear]");
	}
}

REGISTER_SHELL_CMD("ecat", "ecat fault [clear]", EcatCommand);

bool thread_init()
{
	static ::PhyJl1111 phy0 {};
	static ::PhyJl1111 phy1 {};

	static ::EcatPhy* const phys[] = { &phy0, &phy1 };    // Port0/Port1 各挂一颗 JL1111

	// trd 持有 ESC/PHY 实体，按真实端口下标排列后交给门面。
	if (!slave_.Init(&esc_, phys)) {
		DUST_LOG_ERR("ecat: init failed");
		return false;
	}

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
