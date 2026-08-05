/**
 * @file trd_chassis.hpp
 * @author qingyu
 * @brief 四轮全向轮底盘定义
 * @version 0.2
 * @date 2026-08-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "Irq_handlers.h"
#include "to_can_tx.hpp"
#include "dji_c6xx.hpp"
#include <cstdint>

#if CONFIG_USE_POWERMETER
#include "powermeter.hpp"
#endif

namespace instance::chassis
{
    // 功率计 CAN 接收 ID（总功率一个）
    #if CONFIG_USE_POWERMETER

	#define PWRMETER_RX	USER_RX_CAN2
    constexpr uint16_t KPwrMeterId = 0x02;
    inline PowerMeter  PwrMeter {};

    #endif // CONFIG_USE_POWERMETER

	#define CHASSIS_RX  USER_RX_CAN1

    constexpr auto chassis_tx = &user_can1_msgq;

    constexpr uint8_t  N_Wheel = 4;		// 全向轮数量

    // CAN ID 映射
    constexpr uint16_t kDriveCanId[N_Wheel] {0x201, 0x202, 0x203, 0x204};

    // chassis_to_can 消息内的数据槽索引（CAN ID 偏移量）
    constexpr uint8_t  kDriveDataIdx[N_Wheel] {
        static_cast<uint8_t>(kDriveCanId[0] - 0x201),
        static_cast<uint8_t>(kDriveCanId[1] - 0x201),
        static_cast<uint8_t>(kDriveCanId[2] - 0x201),
        static_cast<uint8_t>(kDriveCanId[3] - 0x201),
    };

	inline motor::dji::DjiC620 	chassis_motor_[N_Wheel] {};
}
