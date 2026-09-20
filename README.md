# stm_log

v3.0.0：平台无关的分级日志组件。核心只依赖标准 C，不包含 HAL/CMSIS 头文件，不初始化 UART，
默认不链接 CubeMX、RTT 或 RTOS。UART、RTT、SWO、文件都通过同一个输出回调接入。

保留五级日志、tag 过滤、HEX、颜色、可选文件行号、早期缓冲和总开关。
默认不用动态内存。旧版 UART 初始化接口已移除，迁移方法见下文。

## 最小用法

完整可运行示例见 [example/main.c](example/main.c)：`main()` → `board_init()` → 日志。
主机示例使用 stdout；设备只需替换输出和毫秒时钟，不需要改库或新增 HAL 头文件宏。

```c
#include "stm_log.h"

// 这两个函数由应用提供；output 必须在返回前发送完成或复制数据。
static void output(const char *data, uint16_t len);
static uint32_t millis(void);

void board_init(void)
{
    // 先完成板级时钟和输出外设初始化。
    stm_log_set_tick(millis);  // 可选；不设置时日志时间戳为 0。
    stm_log_init_output(output, STM_LOG_LVL_INFO);
}
```

输出回调按 `len` 使用数据，不保证 NUL 结尾，也不能保存回调收到的临时指针。
默认已附加 CRLF，无需再次加换行。时间戳回调须快速返回，可回绕，不得重入日志。

## UART 与 RTT 接入

UART 仍然可以使用，但 HAL 只出现在应用中：

```c
#include "main.h"      // 本应用选择 MCU/HAL，日志库不包含它。
#include "stm_log.h"

extern UART_HandleTypeDef huart1;
static void uart_output(const char *data, uint16_t len)
{
    // 诊断输出失败时可以计数，但不要在此回调中再次记录日志。
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)data, len, 100U);
}

// HAL/时钟/UART 初始化完成后：
stm_log_set_tick(HAL_GetTick);
stm_log_init_output(uart_output, STM_LOG_LVL_INFO);
```

RTT 只替换输出函数，保留相同初始化：

```c
static void rtt_output(const char *data, uint16_t len)
{
    SEGGER_RTT_Write(0, data, len);
}
stm_log_set_tick(HAL_GetTick); // HAL 仍只在应用层；非 STM32 使用自己的时钟。
stm_log_init_output(rtt_output, STM_LOG_LVL_INFO);
```

不能在 ISR 中调用阻塞日志；异步 DMA 输出必须先复制到自有缓冲，不能直接保存日志栈地址。
本库不报告输出硬件错误，重试/丢弃策略由输出回调负责。

## 过滤与开关

```c
LOGI("app", "version=%s", "1.0.0");
stm_log_set_tag_level("at_comms", STM_LOG_LVL_VERBOSE);
LOGV("at_comms", "<< AT");
stm_log_set_tag_level("at_comms", STM_LOG_LVL_NONE);
stm_log_unset_tag_level("at_comms"); // 恢复全局级别。
LOG_HEX("rx", data, length);
```

tag 指针须保持有效至 unset（推荐字符串常量）。表满时忽略新增项。
长文本、长 tag、长 HEX 行安全截断；HEX 行被截断时不保证展示该行全部字节。

总开关是 `STM_LOG_ENABLED`，与 Debug/Release 无强制绑定：

```cmake
add_subdirectory(Lib/stm_log)
target_compile_definitions(stm_log PUBLIC STM_LOG_ENABLED=1)
target_link_libraries(your_app PRIVATE stm_log)
```

同一个 App 的 `target_link_libraries` 必须统一使用 plain 或 keyword 形式；
CubeMX 已使用 plain 的工程应写 `target_link_libraries(your_app stm_log)`。

设为 0 时 LOGx 宏不求值参数，直接调用日志函数也不输出。
所有调用方与库源码必须使用相同的配置宏；PUBLIC 可自动传递。

## 可选 RTT 依赖

保留 v2.4.0 的 `STM_LOG_WITH_RTT=ON`：优先复用已有 `segger_rtt` target，
其次使用 `STM_LOG_RTT_SOURCE_DIR` 或同级 RTT/segger_rtt 源码，最后按固定提交拉取。
`STM_LOG_RTT_FETCH=OFF` 禁止下载；仓库镜像和配置目录分别用
`STM_LOG_RTT_GIT_REPOSITORY`、`STM_LOG_RTT_CONFIG_DIR` 指定。
该选项只提供链接依赖，不自动配置输出；应用仍需传入自己的 RTT 回调。
默认 OFF，主机和其他平台无需 RTT。

## 配置与运行约束

| 宏 | 默认 | 用途 |
| --- | --- | --- |
| STM_LOG_ENABLED | 1 | 日志总开关 |
| STM_LOG_BUFFER_SIZE | 128 | 单行缓冲，16～65533 字节，含 NUL |
| STM_LOG_USE_COLORS | 1 | ANSI 颜色 |
| STM_LOG_LEVEL_DEFAULT | STM_LOG_LVL_INFO | 默认级别 |
| STM_LOG_MAX_TAGS | 16 | tag 表容量，1～255 |
| STM_LOG_INCLUDE_FILE_LINE | 0 | 文件名和行号 |
| STM_LOG_EARLY_BUFFER_SIZE | 1024 | 未绑定输出时缓存；0 表示丢弃 |
| STM_LOG_AUTO_NEWLINE | 1 | 自动 CRLF |
| STM_LOG_USE_MUTEX | 0 | 保留旧版可选 FreeRTOS 配置路径 |

`stm_log_set_output(NULL)` 暂停输出，按早期缓冲策略处理后续日志；
再次绑定非空输出时刷出缓存。早期缓冲满时丢弃新日志，不覆盖旧记录。
init_output 设置输出和全局级别，不清空已有 tag 配置或时间戳回调。

默认只保证单调用者使用。多任务需要应用串行化所有日志与配置操作。
旧 `STM_LOG_USE_MUTEX=1` 只保留部分配置操作的锁，并不是完整线程安全保证；
会引入 FreeRTOS/动态锁，不能用于无 RTOS 配置。输出和时钟回调均禁止重入。

## 从旧版迁移

- 删除 `STM_LOG_HAL_HEADER` 和 `STM_LOG_LINK_CUBEMX` 配置，它们不再使用。
- `stm_log_init(&huart, level)` 改为应用 UART 回调 + `stm_log_init_output(callback, level)`。
- 继续使用原有 RTT 输出回调；加 `stm_log_set_tick(HAL_GetTick)` 即可保留时间戳。
- `set_output(NULL)` 不再恢复默认 UART；要恢复 UART，显式传回应用的 UART 回调。
- 核心 CMake 不传递 HAL；真正使用 HAL 的应用/适配层需自行链接。
- v3.0.0 是破坏性接口变更；旧标签保持不变。主机测试和集成构建已通过，最新硬件回归尚未完成。

## 主机测试

无需 MCU SDK、HAL 替身或设备：

```sh
cmake -S tests -B build/tests -G Ninja
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

需要 C11 和 C++17 编译器。覆盖默认、无颜色/换行/早期缓冲、文件行号、全关、小缓冲，
以及 C++ 链接和完整示例。旧 MinGW 测试显式启用 C99 printf 实现。
这些软件检查不能代替 UART/RTT 实机验证。

## 许可

保留原有 [MIT License](LICENSE) 和版权声明。
