/**
 * @file Init_entry.hpp
 * @author qingyu
 * @brief 初始化项描述：等级、优先级、函数指针
 * @version 0.1
 * @date 2026-07-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>

/**
 * @brief 初始化阶段
 *
 * Bsp      板级基础能力：时钟、GPIO、总线、中断
 * Module   设备/算法模块：IMU、遥控、电机对象
 * ThreadEarly  早期线程
 * ThreadMid    中期线程
 * ThreadLate   后期线程
 */
enum class InitStage : uint8_t
{
    Bsp         = 0,
    Module      = 1,
    ThreadEarly = 2,
    ThreadMid   = 3,
    ThreadLate  = 4,
};

/**
 * @brief 初始化等级
 */
enum class InitLevel : uint8_t
{
    High = 0,
    Mid  = 1,
    Low  = 2,
};

/**
 * @brief 初始化函数原型
 * @return true=成功, false=失败
 */
using InitFunc = bool (*)();

/**
 * @brief 初始化项描述
 *
 * 每个组件声明一个 InitEntry，链接后形成初始化表，启动器遍历执行。
 */
struct InitEntry
{
    InitFunc     func;      // 初始化函数
    InitStage    stage;     // 所属阶段
    InitLevel    level;     // 高级/中级/低级
    const char*  name;      // 组件名，调试用
};

/**
 * @brief 注册初始化项
 *
 * 每个组件用此宏声明一个 InitEntry，
 * 通过链接段收集形成初始化表，启动器遍历执行。
 *
 * @param fn       初始化函数（符合 InitFunc 签名）
 * @param stage_   InitStage 枚举值（不带 InitStage::）
 * @param level_   InitLevel 枚举值（不带 InitLevel::）
 * @param name_    组件标识，打印用
 */
#define REGISTER_INIT(fn, stage_, level_, name_)                                    \
    static const InitEntry kInitEntry_##fn                                      \
    __attribute__((used, __section__(".user_init"))) = {                        \
        fn, InitStage::stage_, InitLevel::level_, name_                         \
    }
