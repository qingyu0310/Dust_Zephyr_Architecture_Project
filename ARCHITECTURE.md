# project/ 架构说明

## 职责

`project/` 是当前工程的项目装配层。它负责把框架层已经提供的能力组合成某一块板、某一组线程和某一个具体应用。

现在项目差异主要收敛在：

- `boards/`：板级设备树、Kconfig 覆写和烧录配置；
- `thread/`：当前项目启用哪些 RTOS 线程，以及这些线程如何组合模块、topic 和算法。

系统入口、阶段式启动、初始化表遍历和公共中断分发入口已经上移到顶层 `init/`。因此 `project/` 不再持有 `apps/` 目录，也不再负责维护中央启动器。

## 边界

| 管 | 不管 |
| --- | --- |
| 板级配置、alias、overlay、board `.conf` | 不实现底层驱动 |
| 当前项目有哪些线程 | 不定义通用启动机制 |
| 线程如何实例化模块、订阅 topic、发布 topic | 不实现可复用算法 |
| 测试线程如何接入实验功能 | 不维护链接段遍历器 |

项目层应该回答：

```text
这块板上，这个项目，本次镜像要跑哪些业务线程？
```

不应该回答：

```text
全工程所有组件如何注册和启动？
```

这个问题属于 `init/`。

## 目录结构

```text
project/
├── boards/
├── thread/
├── CMakeLists.txt
├── Kconfig
├── README.md
└── ARCHITECTURE.md
```

### boards/

板级配置按 `厂商/板型/` 组织。每个板型通常包含：

| 文件 | 作用 |
| --- | --- |
| `<board>.overlay` | 设备树引脚映射、alias、pinctrl |
| `<board>.conf` | SoC 或板级 Kconfig 覆写 |
| `board.cmake` | 烧录器、OpenOCD 或 runner 配置 |

板级差异应该通过语义 alias 暴露，例如：

```text
remote-uart
imu-spi
imu-pwm
user-can1
```

模块和线程不应该因为换板子就改成直接访问 UART3、SPI2 或某个固定 GPIO。

### thread/

`thread/` 存放当前项目真正运行的 RTOS 线程。

线程层负责：

- 持有当前项目的 static 实例；
- 调用模块层 `Init()` 或 `Start()`；
- 订阅和发布 topic；
- 组合控制链；
- 决定线程周期和优先级；
- 通过注册宏接入 `init/` 启动链。

典型文件结构：

```text
project/thread/imu/
├── trd_imu.hpp
└── trd_imu.cpp
```

`trd_xxx.hpp` 只暴露线程入口，不暴露模块内部实现细节。类声明和可复用逻辑应该沉淀在 `modules/` 或 `algorithm/`。

## 和 init/ 的关系

`init/` 提供启动框架，`project/thread/` 提供项目线程入口。

典型线程在自己的 `.cpp` 中注册：

```cpp
#include "Init_entry.hpp"

static bool thread_init()
{
    return true;
}

static bool thread_start()
{
    return true;
}

REGISTER_INIT(thread_init, EarlyInit, High, "foo_init");
REGISTER_THREAD(thread_start, LateThread, High, "foo_start");
```

这样新增线程时通常不需要修改：

```text
src/main.c
init/Init_entry.cpp
```

是否编译这个线程由 `project/thread/Kconfig` 和 `project/thread/CMakeLists.txt` 决定；编译进来后，注册项才会进入 `.user_init/.user_thread`。

## 新增线程步骤

新增一个项目线程时，通常按下面顺序处理：

1. 在 `project/thread/<name>/` 下新增 `trd_<name>.hpp/.cpp`。
2. 在 `.cpp` 中实现 `thread_init()` 和 `thread_start()`。
3. 使用 `REGISTER_INIT()` / `REGISTER_THREAD()` 注册启动项。
4. 在 `project/thread/Kconfig` 中添加 `CONFIG_TRD_<NAME>`，并 `select` 需要的模块、驱动、算法和 topic。
5. 在 `project/thread/CMakeLists.txt` 中按 `CONFIG_TRD_<NAME>` 添加源文件。
6. 如果需要新设备绑定，在 `project/boards/<vendor>/<board_cfg>/` 中补 overlay 或 `.conf`。

不要为了新增一个普通线程去扩大 `main.c` 或 `init/Init_entry.cpp`。

## thread/test/

`project/thread/test/` 是正式的实验入口，不是临时代码垃圾桶。

它适合：

- 新设备验证；
- 新协议抓帧；
- 电机开环；
- RLS 或模型辨识；
- 调试变量注册；
- 新算法试跑；
- 还没有准备好进入正式业务线程的功能。

验证稳定后，通用能力再沉淀回：

```text
drivers/
modules/
algorithm/
topic/
```

## 依赖关系

`project/` 可以依赖框架层：

```text
drivers/
modules/
algorithm/
topic/
cmd/
init/
```

框架层不应该反向依赖某个具体 `project/thread/`。

当前根 `CMakeLists.txt` 会在项目门禁打开时加入：

```text
drivers/
algorithm/
modules/
topic/
cmd/
init/
project/
```

---

## 当前项目装配

`project/` 不是一个单独的业务库，而是把板级资源、模块能力、topic
契约和线程注册项装配成一个可运行项目。当前线程可以按下面的边界理解：

| 目录 | 类型 | 输入 | 输出或副作用 |
| --- | --- | --- | --- |
| `thread/gpio/` | 基础设施 | GPIO 状态、设备树别名 | 输出控制、输入状态 |
| `thread/remote/` | 设备适配 | `Remote` 模块 | `topic::remote_to` |
| `thread/imu/` | 设备适配 | IMU 设备树和 `ImuManager` | `topic::imu_to` 或姿态数据 |
| `thread/can/` | 基础设施 | `topic::to_can_tx` | CAN 发送 |
| `thread/chassis/` | 业务控制 | 遥控器、底盘电机反馈、功率状态 | 底盘控制输出、CAN 帧 |
| `thread/gimbal/` | 业务控制 | 遥控器、云台电机反馈、IMU 数据 | 云台控制输出、CAN 帧 |
| `thread/pc/` | 基础设施 | PC 链路和调试命令 | 调试数据或控制输入 |
| `thread/test/` | 验证入口 | 被测模块和配置 | Demo、日志、实验结果 |
| `thread/tflm/` | 应用线程 | 应用输入和模型资源 | TFLM 应用行为 |

表中的“输出”表示线程对系统的可见契约，不代表底层硬件一定已经正常。
例如 CAN RX 表中存在反馈处理项，只能说明回调已经注册；过滤器、总线
启动、IRQ 使能和物理链路仍需由驱动路径独立确认。

## 三类线程边界

### 设备适配线程

`remote/` 和 `imu/` 负责把模块内部状态接到项目的数据流：

- 获取设备树中的硬件资源；
- 调用模块初始化和启动接口；
- 将模块快照发布到 topic 或项目规定的数据结构；
- 处理模块返回的 ready、校准和连接状态；
- 不在项目线程中复制协议解析或芯片寄存器操作。

### 基础设施线程

`gpio/`、`can/` 和 `pc/` 提供项目运行所需的通用出口：

- GPIO 线程维护项目需要的输入输出状态；
- CAN 线程作为统一发送出口，消费 `to_can_tx`；
- PC 线程提供调试、观测或上位机交互；
- 这些线程不应反向包含底盘或云台策略。

### 业务控制线程

`chassis/`、`gimbal/` 和 `tflm/` 负责当前项目的业务组合：

- 读取 topic、模块快照和反馈状态；
- 调用 `algorithm/` 中的控制或计算能力；
- 生成控制输出并发布到统一发送队列；
- 维护本项目的模式、限幅、故障降级和控制周期。

业务线程可以依赖模块和算法，但模块不应依赖某个具体业务线程。

## 板级配置边界

板级目录决定资源映射，线程决定资源使用。二者之间通过设备树别名、
Kconfig 选项和公共模块接口连接：

```text
project/boards/<vendor>/<board>/
    ├── *.overlay       设备树节点、别名和外设参数
    ├── *.conf          板级功能选择或覆盖
    ├── board.cmake     构建、烧录和调试参数
    └── openocd.cfg     调试器配置（如需要）
```

线程代码应优先通过 `DT_ALIAS()` 或项目已有的设备树访问方式获取资源。
当一个线程在多个板上复用时，板间差异应落在 overlay 和板级配置中，而
不是在业务代码中增加大量板号判断。

新增板的检查顺序：

1. 确认 SoC、板卡目录和 Zephyr board target。
2. 为 SPI、UART、CAN、PWM、USB 等资源补齐设备树节点和别名。
3. 确认板级配置打开了对应驱动、模块和项目线程。
4. 对照线程中使用的别名逐项检查 `status = "okay"`、引脚和实例。
5. 再检查烧录、调试和构建参数是否属于该板。

## 配置、编译和运行的三次筛选

一个项目线程真正进入运行，需要连续通过三层筛选：

```text
project/Kconfig
        ↓
project/CMakeLists.txt
        ↓
REGISTER_INIT() / REGISTER_THREAD()
        ↓
init 链接段和 System_Startup()
```

| 层次 | 解决的问题 | 常见错误 |
| --- | --- | --- |
| Kconfig | 当前配置是否选择功能 | 选项没有 default、依赖未满足 |
| CMake | 源文件是否进入编译 | 只改 Kconfig，没有加入源文件 |
| 注册表 | 函数是否进入启动链 | 忘记注册、阶段写错、链接段未保留 |

因此，看到源文件存在并不等于线程可运行；看到配置选项存在也不等于
线程已经加入镜像。分析启动问题时应沿着这条链逐层确认。

## 当前数据流

### 输入和控制

```text
UART DMA
   ↓
modules/remotes
   ↓
project/thread/remote
   ↓
topic::remote_to
   ↓
chassis / gimbal
```

遥控器模块把协议差异收敛为统一输入，底盘和云台只消费统一数据。项目
线程不应该为了读取一个通道而直接访问 DR16、SBUS 或 VT 协议字段。

### 反馈和发送

```text
CAN RX
  ↓
modules/motors + modules/powermeter
  ↓
状态快照
  ↓
chassis / gimbal
  ↓
topic::to_can_tx
  ↓
thread/can
  ↓
CAN TX
```

CAN 接收处理和控制发送是两个方向。接收回调应保持短小，控制线程负责
读取稳定快照和执行策略，CAN 发送线程负责统一调用底层发送接口。

### IMU

```text
board overlay
   ↓
thread/imu
   ↓
modules/imu::ImuManager
   ↓
topic/imu_to
   ↓
gimbal / application
```

IMU 线程可以负责当前项目需要的自动校准和启动方式，但不能把芯片寄存器
配置、温控闭环和姿态算法重新复制到 `project/`。

## 新增线程的完整清单

新增线程或把现有模块接入新项目时，至少检查以下项目：

- [ ] 明确线程的输入、输出、周期和失败行为。
- [ ] 确认它属于设备适配、基础设施还是业务控制。
- [ ] 在 `project/Kconfig` 增加开关和依赖。
- [ ] 在 `project/CMakeLists.txt` 增加源文件和 include 路径。
- [ ] 在本地 `.cpp` 中添加 `REGISTER_INIT()`。
- [ ] 在本地 `.cpp` 中添加 `REGISTER_THREAD()`（如果需要循环线程）。
- [ ] 为初始化和线程选择合适的 stage 与 `InitLevel`。
- [ ] 检查线程使用的设备树 alias 在每个目标板上都存在。
- [ ] 检查 topic、消息队列和模块快照的读写方向。
- [ ] 检查线程退出、初始化失败和设备离线时的降级行为。
- [ ] 检查是否错误地把业务策略放进了模块或基础设施线程。

如果新功能只是验证硬件或测量参数，优先接入 `thread/test/`。验证完成
后再决定是否移动到正式业务线程，避免让正式线程承载一次性实验代码。

## 常见错误

### 在 `main()` 中直接初始化项目对象

这会绕过统一注册链，导致阶段、失败等级和链接段都失去作用。项目对象
应在拥有它的 `.cpp` 中注册到 `init/`。

### 只修改 `project/Kconfig`

没有对应的 CMake 源文件映射时，配置项即使可见，实际实现也不会进入镜像。

### 只修改 CMake

没有 Kconfig 门禁时，线程会失去项目级可选性，也容易在不支持的板上
被无条件编译。

### 在项目线程中复制模块协议

这会把板级项目和设备协议绑定在一起，使另一个项目无法复用模块。应扩展
`modules/` 的公共接口或协议适配器。

### 把注册表当成硬件验证

注册项、CAN RX 表和 `.imu` / `.remote` 段只能证明软件入口被保留。硬件
初始化、IRQ、总线状态和真实数据仍需从驱动日志或设备状态确认。

## 维护与验证

修改项目装配后，静态检查顺序建议如下：

1. 用 `rg` 检查 `CONFIG_TRD_*`、源文件路径和注册宏是否成对存在。
2. 检查 `project/boards/` 中目标板的 alias、状态和引脚。
3. 检查线程使用的 topic、消息队列和模块公共接口。
4. 检查对应启动阶段是否满足资源依赖。
5. 检查 CAN、UART、SPI、PWM 等外设的驱动初始化路径。
6. 最后再做构建或硬件验证。

`project/` 的架构文档应记录当前实际装配关系；未来计划、尚未接入的
功能和单个机器人的策略应明确标为计划或应用细节，不能与框架现状混写。
