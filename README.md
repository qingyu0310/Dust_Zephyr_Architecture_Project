# Dust_Zephyr_Architecture_Project

可移植项目单元。嵌入到框架中组成完整系统。

包含三部分：

- **apps/** — 系统入口、启动编排、中断处理
- **boards/** — 板级配置（DTS overlay、Kconfig 覆写、烧录脚本）
- **thread/** — RTOS 线程（GPIO、遥控器、底盘、云台、IMU、PC 通信等）

每个 `project/` 是一个完整的可移植项目单元，复制到其他框架即可使用。
