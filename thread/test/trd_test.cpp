/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief UART 接收线程
 * @version 0.1
 * @date 2026-07-01
 */

#pragma message "Compiling Thread/Test"

#include "thread.hpp"
#include "uart.hpp"
#include "Init_entry.hpp"
#include "dji_c6xx.hpp"
#include "can.hpp"
#include "pid.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(test, LOG_LEVEL_INF);

namespace thread::test {

static Thread<4096> thread_ {};

static UartDma uart3 {};
static k_sem   uart3_sem {};

static DjiC6xx dji_motor {};

static Can user_can1 {};

static alg::pid::Pid pid {};

static float g_speed_target = 0.0f;  // 目标角速度 rad/s，串口更新

static void Task(void*, void*, void*)
{
    uint8_t uart_buf[32];

    for (;;)
    {
        // UART 接收 → 更新目标值
        if (k_sem_take(&uart3_sem, K_NO_WAIT) == 0)
        {
            uint16_t len = uart3.Read(uart_buf, sizeof(uart_buf) - 1);
            if (len > 0) {
                uart_buf[len] = '\0';
                float val;
                if (sscanf((const char*)uart_buf, "%f", &val) >= 1) {
                    g_speed_target = val;
                    printk("target=%.1f\n", (double)g_speed_target);
                }
            }
        }

        auto snap = dji_motor.ReadAll();
        float pid_out = pid.Calc(g_speed_target, snap.omega);

        // PID 输出 → CAN 电流命令
        {
            can_frame tx {};
            tx.id  = 0x200;
            tx.dlc = 8;
            int16_t raw = (int16_t)(pid_out * 16384.0f / 20.0f);
            tx.data[0] = raw >> 8;
            tx.data[1] = raw & 0xFF;
            user_can1.Send(&tx);
        }

        printk("%.1f,%.3f,%.3f\n",
               (double)(snap.omega),
               (double)snap.velocity,
               (double)pid_out);

        k_msleep(1);
    }
}

bool thread_init()
{
    k_sem_init(&uart3_sem, 0, 1);

    RxStream::Config cfg {};
    cfg.buf_size   = 256;
    cfg.rx_timeout = 1000;

    if (!uart3.Init(DEVICE_DT_GET(DT_NODELABEL(uart3)), cfg)) {
        LOG_ERR("uart3 init failed");
        return false;
    }

    uart3.SetNotify(&uart3_sem);
    LOG_INF("uart3 ready");

    {
        DjiC6xx::Config motor_cfg {
            .rx_id          = 0x201,
            .gearbox_ratio  = 90.0f,
            .wheel_r        = 0.05f,
        };
        dji_motor.Init(motor_cfg);
        LOG_INF("dji motor ready (rx_id=0x%04x)", motor_cfg.rx_id);
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

    {
        const device* dev = DEVICE_DT_GET(DT_ALIAS(user_can1));
        if (!device_is_ready(dev)) {
            LOG_ERR("user_can1 not ready");
            return false;
        }
        can_filter filter { .id = 0, .mask = 0, .flags = 0 };
        user_can1.Init(dev, filter);
        user_can1.SetRxCallback([](can_frame &frame, void*) {
            dji_motor.CanCpltRxCallback(frame.data);
        });
        
        LOG_INF("user_can1 ready");
    }

    return true;
}

bool thread_start()
{
    thread_.Start(Task, 7);
    return true;
}

REGISTER_INIT(thread_init,  Module, Low, "test_init");
REGISTER_INIT(thread_start, Thread, Low, "test_start");

} // namespace thread::test
