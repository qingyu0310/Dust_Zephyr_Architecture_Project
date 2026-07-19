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

#include "imu.hpp"
#include "Init_entry.hpp"

namespace thread::imu {

static ::imu::ImuManager imu_ {};

bool thread_init()
{
#ifdef CONFIG_IMU_IDENTIFICATION
    return imu_.Init(::imu::ImuStartMode::AutoCalib);
#else
    return imu_.Init(::imu::ImuStartMode::Normal);
#endif
}

bool thread_start()
{
    return imu_.Start(5);
}

REGISTER_INIT(thread_init,  Module, High, "imu_init");
REGISTER_INIT(thread_start, Thread, High, "imu_start");

} // namespace thread::imu
