/**
 * @file Init_entry.cpp
 * @author qingyu
 * @brief 
 * @version 0.1
 * @date 2026-07-20
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "Init_entry.hpp"
#include "System_startup.h"
#include "zephyr/sys/printk.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(init, LOG_LEVEL_INF);

// 链接段边界，由 linker/tflm_init.ld 定义
extern const InitEntry __user_init_start[];
extern const InitEntry __user_init_end[];

static constexpr struct 
{
    InitStage stage;
    const char* name;
} 
// 线程分三级启动——部分线程依赖其他线程先就绪（如 CAN TX 需早于其他线程）
StageMap[] {
    {InitStage::Bsp,         "BSP"},
    {InitStage::ThreadEarly, "ThreadEarly"},
    {InitStage::Module,      "Module"},
    {InitStage::ThreadMid,   "ThreadMid"},
    {InitStage::ThreadLate,  "ThreadLate"},
};

/**
 * @brief 根据阶段枚举查找阶段名称
 * @param stage 阶段枚举值
 * @return 阶段名字符串，未找到返回 "?"
 */
static const char* StageName(InitStage stage)
{
    for (const auto& m : StageMap)
    {
        if (m.stage == stage) {
            return m.name;
        }
    }
    return "?";
}

static constexpr struct FailAction {
    InitLevel  level;
    bool       halt;
} 
kFailActions[] {
    {InitLevel::High, true},
    {InitLevel::Mid,  false},
    {InitLevel::Low,  false},
};

/**
 * @brief 处理初始化项失败
 *
 * 根据等级决定行为：
 *   High  报错并挂起系统
 *   Mid   报错继续
 *   Low   告警继续
 *
 * @param entry 失败的初始化项
 */
static void HandleInitFail(const InitEntry& entry)
{
    bool halt = false;

    for (const auto& a : kFailActions)
    {
        if (a.level == entry.level) {
            halt = a.halt;
            break;
        }
    }

    if (halt) {
        LOG_ERR("init %s failed, system halted", entry.name);
    } else {
        LOG_WRN("init %s failed", entry.name);
    }

    if (halt) {
        while (1) {}
    }
}

/**
 * @brief 运行指定阶段的所有初始化项
 *
 * 遍历 .user_init 段中所有 InitEntry，
 * 仅执行 stage 匹配的项，按链接顺序。
 *
 * @param stage 待执行的初始化阶段
 */
static void RunStage(InitStage stage)
{
    LOG_ERR("==== stage: %s ====", StageName(stage));

    for (const InitEntry* e = __user_init_start; e < __user_init_end; ++e)
    {
        if (e->stage != stage) {
            continue;
        }
        LOG_ERR("%s", e->name);
        if (!e->func()) {
            HandleInitFail(*e);
        }
    }

    printk("\n");
}

/**
 * @brief 系统启动入口
 *
 * 顺序：Bsp → 早期线程 → Module → 中期线程 → 后期线程。
 * 早期线程在 Module 之前启动，使 CAN 等总线通信先就绪。
 * 由 main() 调用。
 */
void System_Startup(void)
{
    RunStage(InitStage::Bsp);
    k_msleep(1000);
    RunStage(InitStage::ThreadEarly);
    k_msleep(1000);
    RunStage(InitStage::Module);
    k_msleep(1000);
    RunStage(InitStage::ThreadMid);
    k_msleep(1000);
    RunStage(InitStage::ThreadLate);
}



