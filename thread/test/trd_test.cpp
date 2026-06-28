/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief Kalman filter test thread
 * @version 0.1
 * @date 2026-06-28
 */

#pragma message "Compiling Thread/Test"

#include "trd_test.hpp"
#include "thread.hpp"
#include <kalman.hpp>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

namespace thread::test {

static Thread<4096> thread_{};

static void Task(void*, void*, void*)
{
    // 弹簧阻尼系统 Kalman 测试
    constexpr float k  = 10.0f;
    constexpr float c  = 0.5f;
    constexpr float dt = 0.01f;

    alg::filter::Kalman<2, 1> kf;

    Eigen::Matrix2f A;
    A << 1.0f, dt, -k * dt, 1.0f - c * dt;

    Eigen::RowVector2f H;
    H << 1.0f, 0.0f;

    kf.Init(A, H,
            Eigen::Matrix2f::Identity() * 0.01f,
            Eigen::Matrix<float, 1, 1>::Identity() * 0.1f);

    uint32_t cnt = 0;
    for (;;)
    {
        // 模拟位置测量
        Eigen::Matrix<float, 1, 1> z;
        z(0) = 0.0f;

        kf.SetZ(z);

        uint32_t t0 = k_cycle_get_32();
        kf.Predict();
        kf.Update();
        uint32_t t1 = k_cycle_get_32();

        cnt++;
        if (cnt % 1000 == 0) {
            uint64_t ns = k_cyc_to_ns_floor64(t1 - t0);
            printk("[KF] Kalman<2,1> predict+update: %.3f ms\n", (double)ns / 1000000.0);
        }

        k_msleep(1);
    }
}

void thread_init()
{
}

void thread_start(uint8_t prio)
{
    thread_.Start(Task, prio);
}

} // namespace thread::test
