/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 拨弹盘电机辨识 — DJI C6xx + 开环转矩注入 RLS
 * @version 0.1
 * @date 2026-07-22
 */

#pragma message "Compiling Thread/Test"

#include "thread.hpp"
#include "Init_entry.hpp"
#include "dji_c6xx.hpp"
#include "to_can_tx.hpp"
#include "Irq_handlers.h"
#include "motorplant.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(test, LOG_LEVEL_INF);

namespace thread::test {

#define         TEST_RX                         USER_RX_CAN1
constexpr auto *test_tx = &user_can1_msgq;
constexpr auto  test_rx_id    = 0x201;          // DJI 电机反馈 ID
constexpr auto  test_tx_id    = 0x200;          // DJI 电机控制 ID

static Thread<4096> thread_ {};
static motor::dji::DjiC610 dji_motor {};

static alg::identify::motor::MotorPlant g_plant({
    .forget_tau = 1.0f,
    .p_init     = 100.0f,
    .delay_s    = 0.001f,
    .dt_max     = 0.01f,
    .buf_size   = 64,
});

static uint16_t g_rx_seq    = 0;
static float    g_max_omega = 0.0f;

static uint32_t cycle_to_us(uint32_t cyc)
{
    return cyc / (CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC / 1000000);
}

// 开环转矩注入：转矩 (Nm) → CAN raw
static void send_torque(float torque_nm)
{
    constexpr float kTorqueK = 0.18f;
    float current_A = torque_nm / kTorqueK;

    uint32_t t_now = cycle_to_us(k_cycle_get_32());
    g_plant.OnTorqueSend(t_now, torque_nm);

    topic::to_can_tx::Message msg {};
    msg.tx_id = test_tx_id;
    int16_t raw = (int16_t)(current_A / 10.0f * 16384.0f);
    msg.data[0] = raw >> 8;
    msg.data[1] = raw & 0xFF;
    k_msgq_put(test_tx, &msg, K_NO_WAIT);
}

static void Task(void*, void*, void*)
{
    k_msleep(500);

    const float kAmps[] = {0.2f, 0.3f, 0.4f, 0.5f};

    for (uint8_t ai = 0; ai < 4; ai++)
    {
        float amp = kAmps[ai];

        g_plant.Reset();
        g_plant.SetIdentifyMode(true);

        for (uint8_t i = 0; i < 6; i++)
        {
            float torque = (i % 2 == 0) ? amp : -amp;
            for (uint16_t j = 0; j < 5000; j++)
            {
                send_torque(torque);
                k_msleep(1);
            }
        }

        send_torque(0.0f);
        g_plant.SetIdentifyMode(false);

        LOG_INF("=== amp=%f ===  max_omega=%f", (double)amp, (double)g_max_omega);
        LOG_INF("tau=%f K=%f p1=%f p2=%f p4=%f",
                (double)g_plant.GetTau(),
                (double)g_plant.GetK(),
                (double)g_plant.GetP1(),
                (double)g_plant.GetP2(),
                (double)g_plant.GetP4());
        g_max_omega = 0.0f;
    }

    for (;;)
    {
        send_torque(0.0f);
        k_msleep(1);
    }
}

bool thread_init()
{
    {
        motor::dji::DjiC610::Config cfg {
            .rx_id          = test_rx_id,
            .gearbox_ratio  = 90.0f,
            .wheel_r        = 0.05f,
        };
        dji_motor.Init(cfg);
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
        dji_motor.CanCpltRxCallback(data);
        auto snap = dji_motor.ReadAll();
        float omg = snap.omega;
        if (fabsf(omg) > g_max_omega) g_max_omega = fabsf(omg);
        g_plant.OnFeedback(cycle_to_us(k_cycle_get_32()), omg, g_rx_seq++);
    },
    test_motor);

REGISTER_INIT(thread_init,  Module,     Low, "test_init");
REGISTER_INIT(thread_start, ThreadLate, Low, "test_start");

} // namespace thread::test
