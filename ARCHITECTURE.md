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

