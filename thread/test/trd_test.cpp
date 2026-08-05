/**
 * @file trd_test.cpp
 * @author qingyu
 * @brief 测试线程 —— shell log/var 暴力测试业务源（变量注册 + DBG/事件源）
 * @version 0.1
 * @date 2026-06-01
 */

#include "thread.hpp"
#include "Init_entry.hpp"
#include "var.hpp"
#include "log.hpp"
#include <cstdio>

#pragma message "Compiling Thread/Test"

namespace thread::test {

static Thread<2048> thread_ {};

// ===== var 暴力测试变量：覆盖全部 11 种类型 =====
static uint8_t   g_v_u8  = 1;
static int8_t    g_v_i8  = -1;
static uint16_t  g_v_u16 = 2;
static int16_t   g_v_i16 = -2;
static uint32_t  g_v_u32 = 3;
static int32_t   g_v_i32 = -3;
static uint64_t  g_v_u64 = 4;
static int64_t   g_v_i64 = -4;
static float     g_v_f   = 1.5f;
static double    g_v_d   = 2.5;
static bool      g_v_b   = true;

// 业务样变量（模拟真实场景，供 var get 观察变化）
static float     g_vx      = 0.0f;
static float     g_vy      = 0.0f;
static uint32_t  g_loop    = 0;

// ===== 类成员变量测试：真实业务中变量是类成员，验证 REGISTER_SHELL_VAR 对成员的支持 =====
class TestMember
{
public:
    float    vx_    = 0.0f;
    int32_t  cnt_   = 0;
    bool     armed_ = false;
};

static TestMember g_member {};

REGISTER_SHELL_VAR("t_u8",   g_v_u8);
REGISTER_SHELL_VAR("t_i8",   g_v_i8);
REGISTER_SHELL_VAR("t_u16",  g_v_u16);
REGISTER_SHELL_VAR("t_i16",  g_v_i16);
REGISTER_SHELL_VAR("t_u32",  g_v_u32);
REGISTER_SHELL_VAR("t_i32",  g_v_i32);
REGISTER_SHELL_VAR("t_u64",  g_v_u64);
REGISTER_SHELL_VAR("t_i64",  g_v_i64);
REGISTER_SHELL_VAR("t_f",    g_v_f);
REGISTER_SHELL_VAR("t_d",    g_v_d);
REGISTER_SHELL_VAR("t_b",    g_v_b);
REGISTER_SHELL_VAR("vx",     g_vx);
REGISTER_SHELL_VAR("vy",     g_vy);
REGISTER_SHELL_VAR("loop",   g_loop);
// 类成员注册（REGISTER_SHELL_VAR 取成员地址，TypeMap 按成员类型推导）
REGISTER_SHELL_VAR("m_vx",   g_member.vx_);
REGISTER_SHELL_VAR("m_cnt",  g_member.cnt_);
REGISTER_SHELL_VAR("m_arm",  g_member.armed_);

static void Task(void*, void*, void*)
{
    for (;;)
    {
        // DBG 流式源：log on test_loop 后每 100ms 一条（shell 命令可随时插入）
        DUST_LOG_DBG("test_loop", "loop=%lu vx=%.2f vy=%.2f",
                     static_cast<unsigned long>(g_loop),
                     static_cast<double>(g_vx), static_cast<double>(g_vy));

        // DBG 流式源 2：test_vx（验证"同一时间只打一条" + "切换及时顶替"）
        DUST_LOG_DBG("test_vx", "vx=%f", static_cast<double>(g_member.vx_));

        // 事件源：每 1s 一条 INF
        if (g_loop % 10 == 0)
        {
            DUST_LOG_INF("tick %lu", static_cast<unsigned long>(g_loop));
        }

        // 变量周期变化（var get 可观察）
        g_v_u32++;
        g_vx += 0.1f;
        g_vy -= 0.05f;
        g_loop++;

        // 类成员周期变化（m_vx/m_cnt 递增，m_arm 周期翻转）
        g_member.vx_ += 0.2f;
        g_member.cnt_++;
        g_member.armed_ = (g_loop % 20) < 10;
        k_msleep(100);
    }
}

bool thread_init()
{
    // shell 底座（dbg_init, PreInit）已初始化 uart3 + Log，本线程不再初始化
    return true;
}

bool thread_start()
{
    thread_.Start(Task, ThreadPrio::Low);
    return true;
}

REGISTER_INIT  (thread_init,  LateInit,   High, "test_init");
REGISTER_THREAD(thread_start, LateThread, High, "test_start");

} // namespace thread::test
