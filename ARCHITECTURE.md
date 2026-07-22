# project/ 架构说明

## 职责

可移植项目单元。嵌入到框架中组成完整系统。用户主要在这一层完成一个嵌入式应用。

## 边界

| 管 | 不管 |
|----|------|
| 系统入口和启动编排 | 不包含硬件驱动 |
| 板级配置（DTS overlay、Kconfig、烧录脚本） | 不包含算法实现 |
| RTOS 线程逻辑 | 不包含设备管理器类定义 |
| 组合各层模块完成功能 | 不直接操作寄存器 |

## 目录结构

```
project/
├── apps/
├── boards/
├── thread/
├── CMakeLists.txt
├── README.md
└── ARCHITECTURE.md
```

### apps/

系统入口。`main.c` 只调用 `System_Startup()`；真正的启动顺序由 `Init_entry.cpp` 控制：

`Bsp -> ThreadEarly -> Module -> ThreadMid -> ThreadLate`

每个初始化项通过 `REGISTER_INIT()` 注册到 `.user_init` 段，再按阶段执行。

### boards/

板级配置。按 `厂商/板型/` 组织，每个板型包含 `.overlay`（设备树引脚映射）、`.conf`（芯片层 Kconfig）、`board.cmake`（烧录脚本）。

### thread/

RTOS 线程。需要独立线程的模块在此创建子目录，不需要线程的模块不在此暴露实例。

统一接口：`namespace thread::xxx { thread_init(); thread_start(uint8_t prio); }`。各线程通过 Kconfig `select` 拉依赖。

## 文件规范

### apps/

| 文件 | 内容 |
|------|------|
| `Init_entry.cpp` | 阶段式初始化编排 |
| `System_startup.h` | 初始化函数声明 |
| `Irq_handlers.cpp` | 中断处理函数 |

### boards/

每板一个目录，包含：

| 文件 | 作用 |
|------|------|
| `<board>.overlay` | 设备树引脚映射、alias |
| `<board>.conf` | SoC 层 Kconfig |
| `board.cmake` | 烧录脚本路径 |

### thread/

新增线程步骤：

| 文件 | 内容 |
|------|------|
| `trd_xxx.hpp` | `namespace thread::xxx { thread_init(); thread_start(uint8_t prio); }` |
| `trd_xxx.cpp` | 局部 static 实例 + 实现 |

规则：
- 类声明在模块层的 hpp 中，不在 trd_xxx.hpp 中暴露实现细节
- `thread_start` 中通过 `IsReady()` 检查后再启动
- 核心类的 `Start()` 方法内部自带 `ready_` 防呆检查
- 在 `thread/Kconfig` 中添加开关，`select` 模块依赖
- 在 `thread/CMakeLists.txt` 中添加 `CONFIG_TRD_XXX` 编译段
- 在 `apps/Init_entry.cpp` / `Init_entry.hpp` 中补齐对应阶段和初始化项

## 依赖关系

`project/` 依赖所有框架层（algorithm、drivers、modules、topic、cmd）。通过 Kconfig 的 `select` 机制拉取实际使用的模块，未被选中的代码不参与编译。

## 被谁调用

- 根 CMakeLists.txt 通过 `add_subdirectory(${PROJ_DIR})` 嵌入
- 框架层完全不依赖 `project/`
