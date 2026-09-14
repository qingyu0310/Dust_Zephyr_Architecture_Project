/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 测试线程 —— DJI M2006 开环电流驱动（遥控俯仰通道给电流，1ms 周期发 CAN 控制帧）
 * @version 0.6
 * @date 2026-09-14
 */

#include "thread.hpp"
#include "Init_entry.hpp"
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include "can.hpp"
#include "dji_c6xx.hpp"
#include "Irq_handlers.h"
#include "remote_to.hpp"

#pragma message "Compiling Thread/Test"

namespace thread::test {

// C610 控制帧 ID 0x200 的 [0-1] 字节对应反馈 ID 0x201 的电机
static constexpr uint16_t kMotorRxId    = 0x201;
static constexpr uint16_t kMotorTxId    = 0x200;

static constexpr float    kCurrentA     = 1.0f;                 // 俯仰满舵电流 (A)
static constexpr float    kCurrentScale = 16384.0f / 10.0f;     // C610 电流缩放：A → raw
static constexpr uint32_t kPeriodMs     = 1;                    // 控制帧发送周期

static Thread<2048>        thread_ {};
static Can                 can_    {};
static motor::dji::DjiC610 motor_  {};

static void Task(void*, void*, void*)
{
    can_frame tx {
        .id  = kMotorTxId,
        .dlc = 8,
    };

    for (;;)
    {
        const zbus_channel *chan = nullptr;
        zbus_sub_wait(&sub_remote_to, &chan, K_NO_WAIT);
		
        if (chan) {
            topic::remote_to::Message rx {};
            zbus_chan_read(chan, &rx, K_NO_WAIT);

            // 俯仰通道（归一化 [-1,1]）→ 电流 (A) → C610 raw
            const float   current = rx.chassisy * kCurrentA;
            const int16_t raw     = static_cast<int16_t>(current * kCurrentScale);
            tx.data[4] = static_cast<uint8_t>(raw >> 8);
            tx.data[5] = static_cast<uint8_t>(raw & 0xFF);
        }

        can_.Send(&tx);
        k_msleep(kPeriodMs);
    }
}

bool thread_init()
{
    // M2006 + C610 反馈帧
    motor::dji::DjiC610::Config motor_cfg {};
    motor_cfg.rx_id = kMotorRxId;
    motor_.Init(motor_cfg);

    const device* dev = DEVICE_DT_GET(DT_ALIAS(user_can1));
    const can_filter filter { .id = 0, .mask = 0, .flags = 0 };
    if (!can_.Init(dev, filter)) {
        return false;
    }
    can_.SetRxCallback(user_can1_rx_callback);

    return true;
}

bool thread_start()
{
    thread_.Start(Task, ThreadPrio::High, nullptr, "test");
    return true;
}

REGISTER_INIT  (thread_init,  LateInit, High, HaltOnFail, "test_init");
REGISTER_THREAD(thread_start, LateThread, "test_start");

// M2006 反馈帧接收注册（0x201）
CAN_RX_HANDLER(USER_RX_CAN1, kMotorRxId, [](uint8_t *data) { motor_.CanCpltRxCallback(data); }, motor0);

} // namespace thread::test
