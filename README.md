# Dust_Zephyr_Architecture_Project

可移植项目单元。嵌入到框架中组成完整系统。

在框架层能力已经具备的前提下，新建项目或移植项目，原则上主要只需要修改 `project/`。

包含两部分：

- **boards/** — 板级配置（DTS overlay、Kconfig 覆写、烧录脚本）
- **thread/** — RTOS 线程（GPIO、遥控器、底盘、云台、IMU、PC 通信、Demo/测试等）

每个 `project/` 是一个完整的可移植项目单元，复制到其他框架即可使用。

系统入口、启动阶段、注册表遍历和中断分发入口已经上移到顶层 `init/`。项目线程仍然通过 `REGISTER_INIT()` / `REGISTER_THREAD()` 本地注册到统一启动链。

其中 `thread/test/` 专门用于快速 Demo、设备验证、参数实验和临时功能接入。
它可以直接选择已有模块并进入当前 Kconfig/CMake/初始化链，不必为了一个实验污染正式业务线程。
