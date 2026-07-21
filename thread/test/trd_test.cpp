/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief UART 接收线程
 * @version 0.1
 * @date 2026-07-01
 */

#pragma message "Compiling Thread/Test"

#include "thread.hpp"
#include "Init_entry.hpp"
#include "dm.hpp"
#include "pid.hpp"
#include "to_can_tx.hpp"
#include "Irq_handlers.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(test, LOG_LEVEL_INF);

namespace thread::test {

#define         TEST_RX                         USER_RX_CAN1
constexpr auto *test_tx = &user_can1_msgq;
constexpr auto  test_rx_id         = 0x13;     // 电机反馈帧 CAN ID（= master_id），不是发送用的 can_id

static Thread<4096> thread_ {};


static DmMotor dm_motor {};

static alg::pid::Pid pid {};

static float g_torque_target = 0.0f;  // 目标角速度 rad/s，串口更新

static void Task(void*, void*, void*)
{
    for (;;)
    {
        auto snap = dm_motor.ReadAll();
        float pid_out = pid.Calc(g_torque_target, snap.torque);

        // PID 输出 → topic → CAN TX
        {
            topic::to_can_tx::Message msg {};
            msg.tx_id = dm_motor.GetTxId();
            dm_motor.SetTargetTorque(g_torque_target);
            dm_motor.PackCtrlFrame(msg.data);
            k_msgq_put(test_tx, &msg, K_NO_WAIT);
        }

        printk("%f,%f,%f\n",
               (double)(snap.torque),
               (double)snap.angle,
               (double)pid_out);

        k_msleep(1);
    }
}

bool thread_init()
{
    {
        DmMotor::Config motor_cfg {
            .ctrl_met       = ControlMethon::Mit,
            .can_id         = 0x01,
            .master_id      = 0x13,
            .gearbox_ratio  = 1,
            .wheel_r        = 0.034f,
            .kp             = 0,
            .kd             = 0,
            .PMAX           = 12.56637,
            .VMAX           = 45,
            .TMAX           = 10,
        };
        dm_motor.Init(motor_cfg);
        LOG_INF("dm motor ready (id=0x%02x)", motor_cfg.can_id);
    }

    {
        alg::pid::Pid::Config pid_cfg {};
        pid_cfg.kp     = 1.0f;
        pid_cfg.ki     = 0.0f;
        pid_cfg.kd     = 0.0f;
        pid_cfg.outMax = 10.0f;
        pid.Init(pid_cfg);
        LOG_INF("pid ready");
    }

    // 发送电机使能
    {
        topic::to_can_tx::Message msg {};
        msg.tx_id = dm_motor.GetTxId();
        dm_motor.PackCmdFrame(msg.data, DmMotor::Cmd::Enable);
        k_msgq_put(test_tx, &msg, K_NO_WAIT);
        k_busy_wait(1000000);
        LOG_INF("dm motor enable");
    }

    return true;
}

bool thread_start()
{
    thread_.Start(Task, ThreadPrio::Low);
    return true;
}

CAN_RX_HANDLER(TEST_RX, test_rx_id,
    [](uint8_t *data) {
        dm_motor.CanCpltRxCallback(data);
    }, 
    test_motor);

REGISTER_INIT(thread_init,  Module,     Low, "test_init");
REGISTER_INIT(thread_start, ThreadLate, Low, "test_start");


} // namespace thread::test
