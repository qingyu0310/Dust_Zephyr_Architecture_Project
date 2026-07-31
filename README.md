# project/ - 项目装配层

`project/` 是面向一个具体板卡和应用的可移植项目单元。它不重新实现
驱动、设备协议或通用算法，而是把已经存在的框架能力装配成当前产品的
板级配置、线程集合和数据流。

移植到另一块板或另一个机器人时，优先在这里完成选择和适配。只有当设备
能力本身不存在时，才回到 `drivers/` 或 `modules/` 增加通用实现。

## 目录结构

```text
project/
├── boards/
│   ├── hpm/
│   │   ├── hpm5361icb/
│   │   └── hpm6e00evk/
│   └── st/
│       ├── board_rm_c/
│       └── puzhong/
├── thread/
│   ├── can/       CAN 发送线程
│   ├── chassis/   底盘控制线程
│   ├── gimbal/    云台控制线程
│   ├── gpio/      GPIO 输入输出线程
│   ├── imu/       IMU 模块适配线程
│   ├── pc/        PC 通信线程
│   ├── remote/    遥控器模块适配线程
│   ├── test/      Demo、验证和实验线程
│   └── tflm/      TFLM 应用线程
├── CMakeLists.txt
└── Kconfig
```

`boards/` 描述“这块板是什么”，`thread/` 描述“这个项目运行什么”。
启动器、阶段枚举、注册表遍历和 CAN RX 公共分发属于顶层 `init/`，不在
`project/` 中重复实现。

## boards/

板级目录通常包含以下文件：

| 文件 | 用途 |
| --- | --- |
| `*.overlay` | 设备树节点、别名、引脚和外设实例 |
| `*_defconfig` 或 `*.conf` | 板级默认 Kconfig 选择 |
| `board.cmake` | 构建或烧录时的板级参数 |
| `openocd.cfg` 或相关脚本 | 调试器和烧录器配置 |

板级适配的搜索顺序应保持稳定：

1. 先在 `boards/<vendor>/<board>/` 确认设备树别名和外设实例。
2. 再检查 `project/Kconfig` 与板级配置是否打开对应模块和线程。
3. 最后检查 `project/CMakeLists.txt` 是否把线程源文件和头文件路径加入
   当前配置。

常见别名包括 `user-can1`、`remote-uart`、`imu-spi`、`imu-pwm` 和
`pc-usb`。线程通过设备树别名获取资源，避免把具体引脚和控制器编号
硬编码在业务代码中。

## thread/

当前项目线程的职责可以按三类理解：

| 类型 | 线程 | 主要职责 |
| --- | --- | --- |
| 设备适配 | `remote/`、`imu/` | 调用模块接口，把设备状态接入 topic 或项目数据流 |
| 基础设施 | `gpio/`、`can/`、`pc/` | 提供项目需要的输入输出、发送和调试通道 |
| 业务控制 | `chassis/`、`gimbal/`、`tflm/` | 读取输入和反馈，运行控制策略并发布输出 |

`test/` 是验证入口，不等同于正式业务线程。它适合验证设备、观察
初始化顺序、做参数实验和接入临时功能；验证完成后应决定功能是否进入
正式线程或模块。

## 当前启动关系

项目线程在本地声明初始化和线程注册项，`init/` 统一遍历这些注册项：

| 项目部分 | 当前阶段示例 | 说明 |
| --- | --- | --- |
| GPIO 输出 | `PreInit` / `PreThread` | 先准备基础输出资源 |
| 遥控器、CAN、PC | `PreInit` 或 `EarlyInit` | 建立输入和基础通信 |
| IMU | `EarlyInit` / `LateThread` | 初始化模块并持续采样 |
| 底盘、云台 | `MidInit` / `MidThread` | 在输入和反馈可用后运行控制 |
| 测试 | `LateInit` / `LateThread` | 接入验证和实验功能 |
| TFLM | `AppInit` / `AppThread` | 运行应用层模型或最终业务 |

阶段只表达粗粒度的启动先后。线程优先级、周期、队列容量和设备 ready
检查仍由项目线程和对应模块明确处理。

## 主要数据流

### 遥控器到控制线程

```text
remote module
      ↓
project/thread/remote
      ↓
topic/remote_to
      ↓
project/thread/chassis + project/thread/gimbal
```

遥控器协议由 `modules/remotes/` 解析，项目线程只消费统一的遥控器数据。
底盘和云台可以分别读取同一份 topic，不应共享协议解析状态。

### 控制输出到 CAN

```text
chassis / gimbal
      ↓
topic/to_can_tx
      ↓
project/thread/can
      ↓
Can::Send()
```

控制线程负责计算和生成发送数据，CAN 线程负责按统一出口发送。电机和
功率计的接收反馈则通过模块注册的 CAN RX 回调进入状态缓存，再由业务
线程读取。

### IMU 到应用

```text
board aliases
      ↓
project/thread/imu
      ↓
ImuManager
      ↓
topic/imu_to 或应用消费者
```

IMU 线程是项目与 IMU 模块之间的适配边界。芯片型号、采样和温控细节
应留在 `modules/imu/`，项目线程只决定当前项目需要的启动方式和输出契约。

## Kconfig 与 CMake

项目配置和编译入口形成一条完整路径：

```text
project/Kconfig
        ↓
CONFIG_TRD_* / CONFIG_CMD_* / board options
        ↓
project/CMakeLists.txt
        ↓
project/thread/<name>/*.cpp
        ↓
REGISTER_INIT() / REGISTER_THREAD()
        ↓
init/System_Startup()
```

`project/Kconfig` 负责声明当前项目可选的线程和命令，`project/CMakeLists.txt`
负责把被选择的实现加入构建。只在 CMake 中加入源文件而没有 Kconfig 门禁，
或者只加 Kconfig 而没有 CMake 映射，都会造成配置看似存在但运行路径不完整。

## 新增项目线程

新增一个线程时，按下面的顺序检查：

1. 明确线程是设备适配、基础设施还是业务控制，先定义输入、输出和周期。
2. 在 `project/thread/<name>/` 放置线程实现和私有头文件。
3. 在 `project/Kconfig` 增加开关和必要依赖。
4. 在 `project/CMakeLists.txt` 增加条件源文件和 include 路径。
5. 在拥有该线程资源的 `.cpp` 中声明 `REGISTER_INIT()` 和
   `REGISTER_THREAD()`。
6. 选择合适的 `InitStage`、`ThreadStage`、`InitLevel`、线程优先级和周期。
7. 如果线程读写 topic、消息队列或模块状态，写清楚数据所有权和更新频率。
8. 如果线程依赖板级资源，检查所有支持板的设备树别名和配置。
9. 使用静态搜索确认 Kconfig、CMake、注册项和实际源文件一致。

不要为了新线程修改 `src/main.c` 或扩大 `init/System_Startup()` 的固定流程。
项目线程应通过本地注册进入统一启动链。

## 移植项目

移植到新板时，优先复制并修改一个现有板级目录：

```text
project/boards/<vendor>/<new_board>/
```

然后依次完成：

- 设备树节点和别名；
- 板级默认配置；
- 调试和烧录参数；
- `project/Kconfig` 中的线程选择；
- `project/CMakeLists.txt` 中的源文件映射；
- 各线程对设备树别名和模块接口的检查。

如果只是更换芯片或协议实现，不应把协议解析复制到 `project/thread/`；
应优先复用或扩展 `modules/` 的公共能力。

## 边界速查

| 需求 | 首选位置 |
| --- | --- |
| 修改引脚、SPI、UART、CAN 节点 | `project/boards/` |
| 选择当前项目启用哪些线程 | `project/Kconfig` |
| 映射线程源文件 | `project/CMakeLists.txt` |
| 设备协议和状态模型 | `modules/` |
| 纯计算和控制算法 | `algorithm/` |
| 线程间数据契约 | `topic/` |
| 启动阶段和注册表遍历 | `init/` |

更完整的依赖、注册和运行时说明见
[ARCHITECTURE.md](ARCHITECTURE.md)。
