/**
 * @file trd_imu.cpp
 * @author qingyu
 * @brief IMU 线程 — 数据采集、加热控温、姿态解算
 * @version 0.1
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma message "Compiling Thread/Imu"

#include "trd_imu.hpp"
#include "imu.hpp"

namespace thread::imu {

static ::imu::ImuManager imu_ {};

void thread_init()
{
#ifdef CONFIG_IMU_IDENTIFICATION
    imu_.Init(::imu::ImuStartMode::OpenIdent);
#else
    imu_.Init(::imu::ImuStartMode::Normal);
#endif
}

void thread_start(uint8_t prio)
{
    if (!imu_.IsReady()) {
        return;
    }

    imu_.Start(prio);
}

} // namespace thread::imu
