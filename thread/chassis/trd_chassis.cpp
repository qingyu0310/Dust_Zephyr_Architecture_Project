/**
 * @file trd_chassis.cpp
 * @author qingyu
 * @brief 底盘控制线程 — 1ms 周期：遥控 → 运动学 → PID → 功率分配 → 发布
 * @version 0.3
 * @date 2026-08-04
 *
 * ## 坐标系
 *
 * 右手系，z 轴向上：
 *
 *       +y (前)
 *       ↑
 *       |
 *       O────→ +x (右)
 *
 * 车体旋转 ω 绕 z 轴，逆时针为正。
 *
 * ## 控制流
 *
 * ReadRemote()             读取遥控器 zbus → vx/vy/vw
 *     ↓
 * UpdateTarget()           逆向运动学（四角麦轮，参考 Dust）
 *     ↓
 * ControlCalculate()       PID：速度环直接输出电流
 *     ↓
 * PowerAlloc()             功率预测 + 分配（全向轮单组）
 *     ↓
 * FramePublish()           组帧 → zbus → can_tx
 *
 * 逆向运动学（照 Dust：v 轮 = (±vx ± vy)·√2 + ω）：
 *
 *   轮0: v = (-vx + vy)·√2 + ω
 *   轮1: v = (-vx - vy)·√2 + ω
 *   轮2: v = ( vx - vy)·√2 + ω
 *   轮3: v = ( vx + vy)·√2 + ω
 *
 * @copyright Copyright (c) 2026
 */

#include <cstdio>
#include "trd_chassis.hpp"
#include "timer.hpp"
#include "remote_to.hpp"
#include "thread.hpp"
#include "Init_entry.hpp"
#include "to_can_tx.hpp"
#include "power_ctrl.hpp"
#include "pid.hpp"
#include "zephyr/zbus/zbus.h"
#include "math.h"
#include "var.hpp"

#pragma message "Compiling Thread/Chassis"


namespace thread::chassis {

using namespace instance::chassis;

static Thread<1024 * 4> thread_{};

// 电机参数
static constexpr uint8_t  kTotalBudget      = 60;                               // 底盘总功率预算 W

// 速度限幅
static constexpr float KMaxMoveVelocity     = 0.8f;                             // 最大移动线速度 m/s
static float KMaxRotationOmega    = 3.0f;                             // 最大旋转角速度 rad/s

// 方向补偿（电机安装方向导致编码器正方向与底盘坐标系相反）
static constexpr int8_t kDriveSign[N_Wheel] = { 1, 1, 1, 1};    // 行进方向，需实测

// 控制算法
static alg::pid::Pid chassis_pid_[N_Wheel] {};

// 功率控制器（全向轮 4 电机单组）
static alg::power_ctrl::PowerCtrl<N_Wheel> ChassisPwrCtrl {};

// 运动学解算输出：各轮线速度目标
static float g_wh_target[N_Wheel] {};

// 各轮输出电流（注释 PowerAlloc() 就是 PID 直接输出，未限幅）
static float g_wh_current[N_Wheel] {};

// 底盘速度指令
static float g_vx = 0.0f, g_vy = 0.0f, g_vw = 0.0f;

static float pre_current = 0.0f, late_current = 0.0f;

/**
 * @brief 从 zbus 读取遥控器数据 → vx/vy/vw
 */
static void ReadRemote()
{
	constexpr float kRotationRadius = 0.17;	

    static topic::remote_to::Message msg{};
    const zbus_channel *chan = nullptr;

    zbus_sub_wait(&sub_remote_to, &chan, K_NO_WAIT);
	
    if (chan) {
        zbus_chan_read(chan, &msg, K_NO_WAIT);
        g_vx =  msg.chassisx  * KMaxMoveVelocity;
        g_vy =  msg.chassisy  * KMaxMoveVelocity;
        g_vw = (msg.chassis_mode == topic::remote_to::ChassisMode::Spin) ? KMaxRotationOmega * kRotationRadius: 0.0f;
    }
}

/**
 * @brief 逆向运动学解算
 *
 * 对每轮：v = (±vx ± vy)·√2 + ω，直接映射出底盘 4 轮线速度目标，
 * 再乘以方向补偿。
 */
static void UpdateTarget()
{
    constexpr float kSqrt2 = 1.41421356f;

    g_wh_target[0] = (-g_vx + g_vy) * kSqrt2 + g_vw;
    g_wh_target[1] = (-g_vx - g_vy) * kSqrt2 + g_vw;
    g_wh_target[2] = ( g_vx - g_vy) * kSqrt2 + g_vw;
    g_wh_target[3] = ( g_vx + g_vy) * kSqrt2 + g_vw;

    for (uint8_t wi = 0; wi < N_Wheel; wi++) {
        g_wh_target[wi] *= kDriveSign[wi];
    }
}

/**
 * @brief PID 控制
 *
 * 每轮单 PID：速度环直接输出电流。
 */
static void ControlCalculate()
{
    for (uint8_t wi = 0; wi < N_Wheel; wi++)
    {
        const auto snap = chassis_motor_[wi].ReadAll();

        chassis_pid_[wi].SetTarget(g_wh_target[wi]);
        chassis_pid_[wi].SetNow(snap.velocity);
        const float current_ref = chassis_pid_[wi].Calc();

        g_wh_current[wi] = current_ref;      							// 未分配时的电流

        ChassisPwrCtrl.SetTarget(wi, current_ref);
        ChassisPwrCtrl.SetMotorData(wi, snap.torque, snap.omega, chassis_pid_[wi].GetError());
    }

	pre_current = g_wh_current[0];
}

/**
 * @brief 功率预测 + 分配（全向轮单组）
 *
 * 4 个驱动电机统一预测功率，按总预算分配。
 */
static void PowerAlloc()
{
    #if CONFIG_USE_POWERMETER
    {
        ChassisPwrCtrl.SetMeasuredPower(PwrMeter.GetPower(), PwrMeter.HasFrame());
    }
    #endif // CONFIG_USE_POWERMETER
	
    ChassisPwrCtrl.Predict();
    ChassisPwrCtrl.Allocate(kTotalBudget);

    for (uint8_t wi = 0; wi < N_Wheel; wi++) {
        g_wh_current[wi] = ChassisPwrCtrl.GetLimitedCurrent(wi);		// 分配后的电流
    }
	
	late_current = g_wh_current[0];
}

/**
 * @brief 组帧 → 发布到 CAN 发送 topic
 */
static void FramePublish()
{
	constexpr float 	kCurrentScale = 16384.0f / 20.0f;                   // 电流缩放系数
	constexpr uint16_t 	kChassisTxId  = 0x200;                            	// 底盘can发送id

	topic::to_can_tx::Message msg{};
    auto set_out = [&](uint8_t idx, float current_A) 
	{
        int16_t raw = static_cast<int16_t>(current_A * kCurrentScale);
        msg.data[idx * 2 + 0] = static_cast<uint8_t>(raw >> 8);
        msg.data[idx * 2 + 1] = static_cast<uint8_t>(raw & 0xFF);
    };

    for (uint8_t wi = 0; wi < N_Wheel; wi++)
    {
        set_out(kDriveDataIdx[wi], g_wh_current[wi]);
    }

    msg.tx_id = kChassisTxId;
    // k_msgq_put(chassis_tx, &msg, K_NO_WAIT);
}

/**
 * @brief 底盘控制主循环
 *
 * 顺序：遥控 → 运动学 → PID → 功率 → 发布，固定 1ms 周期。
 */
static void Task(void*, void*, void*)
{
    static constexpr uint32_t kPeriodMs = 1;

	Timer log_timer(50);

    for (;;)
    {
        const int64_t tick_start = k_uptime_get();

		log_timer.Update();

        ReadRemote();
        UpdateTarget();
        ControlCalculate();
        PowerAlloc();
        FramePublish();

		log_timer.Clock([](){
			// printk("%f,%f\n", (double)PwrMeter.GetPower(), (double)ChassisPwrCtrl.GetTotalPowerClamped());
		});

        const int64_t elapsed = k_uptime_get() - tick_start;
        const int64_t remain  = static_cast<int64_t>(kPeriodMs) - elapsed;
        if (remain > 0) {
            k_msleep(remain);
        }
    }
}

bool thread_init()
{
	constexpr float kTorqueK = 0.246f;              	// M3508 减速箱输出轴转矩常数 = 额定2.46N·m/10A（2026-08-05 手册额定数据）

    // 功率计初始化
    #if CONFIG_USE_POWERMETER
    {
        PwrMeter.Init(KPwrMeterId);
    }
    #endif

    // 功率预测模型初始化（全向轮单组）
    {
        constexpr float k1 = 2.3f;                     // τ² 铜损系数初值（堵转标定 ~2.0，在线单参数收敛，2026-08-05）
        constexpr float k2 = 0.128f;                   // |ω| 线性损耗系数（架起三档空转标定：低 0.112 / 中0.113 / 高0.143 → 合并 0.128，2026-08-05）
        constexpr float k3 = 3.1f;

        // ── v4 模型（fixK2 单参数在线辨 k1）───────────────────────────
        // 2026-08-05 v4：双参数 RLS 实测反相关（r=-0.84）不收敛——驾驶工况 τ² 与 |ω| 同涨同落，
        // 共线是工况本质非模型形式。改 fixK2=true 只在线辨 k1（v2 验证单参数收敛）。
        // K2=0.128 架起三档空转标定冻结，Kt=1.0、K3=3.1 固定。见 doc/功率模型诊断与方案定稿.md。

        alg::power_ctrl::PowerCtrl<N_Wheel>::Config cfg{};
        cfg.k1Init         = k1;
        cfg.k2Init         = k2;
        cfg.k3             = k3;
		cfg.torqueK        = kTorqueK;
        cfg.errUpper       = 500.0f;
        cfg.errLower       = 0.001f;
        cfg.rlsLambda      = 0.99999f;              // RLS 遗忘因子（接近 1 防膨胀；Init 调 SetLambda 生效）
        cfg.pInit          = 1e-5f;                 // RLS 协方差初值
        cfg.excMinAbsOmega = 5.0f;                 	// 激励门控  Σ|ω| < 5（约均速<1.25rad/s）且
        cfg.excMinTau2     = 0.05f;                 //          Στ² < 0.05（空载 0.004 之上）时跳过
        cfg.fixK2          = true;                  // 固定 K2，单参数只辨 k1（双参数共线不收敛）
        cfg.deadzonePower  = 5.0f;                  // RLS 更新死区：|P_meas|<5W 跳过（港科大；可调到 2W）
        cfg.kFloor         = 1e-5f;                 // 辨识系数下限钳位（防发散为负）
        cfg.skipNegPower   = true;                  // 停车/急刹预测功率为负时跳过更新，防污染 K1
        cfg.rlsEnable      = false;                 // 在线单参数辨 k1
        cfg.tauOmegaEnable = true;                  // τ·ω 机械功率项是模型物理项，始终保留
        cfg.powerMax       = kTotalBudget;          // 总功率钳制上限
        ChassisPwrCtrl.Init(cfg);
    }

    // 4 个驱动电机 + PID 初始化
    for (uint8_t wi = 0; wi < N_Wheel; wi++)
    {
        constexpr float kWheelR       = 0.077f;
        constexpr float kGearboxRatio = 268.f / 17.f;

        motor::dji::DjiC620::Config motor_cfg {};
        motor_cfg.rx_id         = kDriveCanId[wi];
        motor_cfg.wheel_r       = kWheelR;
        motor_cfg.gearbox_ratio = kGearboxRatio;
		motor_cfg.torque_k 		= kTorqueK;

        alg::pid::Pid::Config speed_cfg {};
        speed_cfg.kp  = 10.0f;
        speed_cfg.ki  = 1.0f;
        speed_cfg.kd  = 0.0f;

        chassis_motor_[wi].Init(motor_cfg);
        chassis_pid_[wi].Init(speed_cfg);
    }

    return true;
}

bool thread_start()
{
    thread_.Start(Task, ThreadPrio::High, nullptr, "chassis");
    return true;
}

REGISTER_INIT  (thread_init,  LateInit,    High, HaltOnFail, "chassis_init");
REGISTER_THREAD(thread_start, LateThread, "chassis_start");

// CAN 接收注册（4 轮电机反馈）
CAN_RX_HANDLER(CHASSIS_RX, kDriveCanId[0], [](uint8_t *data) { chassis_motor_[0].CanCpltRxCallback(data); }, motor0);
CAN_RX_HANDLER(CHASSIS_RX, kDriveCanId[1], [](uint8_t *data) { chassis_motor_[1].CanCpltRxCallback(data); }, motor1);
CAN_RX_HANDLER(CHASSIS_RX, kDriveCanId[2], [](uint8_t *data) { chassis_motor_[2].CanCpltRxCallback(data); }, motor2);
CAN_RX_HANDLER(CHASSIS_RX, kDriveCanId[3], [](uint8_t *data) { chassis_motor_[3].CanCpltRxCallback(data); }, motor3);

// 功率计 CAN 接收注册
#if CONFIG_USE_POWERMETER
CAN_RX_HANDLER(PWRMETER_RX, KPwrMeterId, [](uint8_t *data) { PwrMeter.CanCpltRxCallback(data); }, powermeter);
#endif // CONFIG_USE_POWERMETER

REGISTER_SHELL_VAR("KMaxRotationOmega", KMaxRotationOmega);

} // namespace thread::chassis
