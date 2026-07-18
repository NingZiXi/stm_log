# stm_log

STM32 HAL 专用分级日志组件。单文件、可静态库集成。

---

## 特性

- 5 级（VERBOSE / DEBUG / INFO / WARN / ERROR）+ NONE 关闭
- **per-tag 级别控制**（运行时）
- **可选 `[file:line]`** 输出（编译期开关）
- **自定义输出 callback**（切到 RTT / SWO / USB CDC / 文件…）
- **HEX buffer 打印**（`LOG_HEX` / `LOG_HEXD`）
- **早期 log + 自动 flush**（`stm_log_init()` 之前的 LOGx 进 ring buffer）
- **编译期全关**（`STM_LOG_ENABLED=0`，用于量产 release）
- **FreeRTOS 多任务安全**（编译期开关 `STM_LOG_USE_MUTEX=1`，裸机项目零开销）
- 时间戳自动取 `HAL_GetTick()`
- 可选 ANSI 颜色

## Quickstart

```c
#include "stm_log.h"

static const char *TAG = "main";

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_USART1_UART_Init();

    stm_log_init(&huart1, STM_LOG_LVL_INFO);            /* 绑定 UART + 全局级别 */
    stm_log_set_tag_level("wifi", STM_LOG_LVL_WARN);    /* wifi tag 只看 WARN+ */

    LOGI(TAG, "boot, heap=%u", xPortGetFreeHeapSize());
    LOGE(TAG, "uart tx failed");
}
```

输出示例：

```
I (1234) main: boot, heap=12345
E (5678) main: uart tx failed
```

颜色：ERROR=红 / WARN=黄 / INFO=绿 / DEBUG=灰 / VERBOSE=青

## API 速查

```c
/* 初始化 + 级别 */
void                 stm_log_init(UART_HandleTypeDef *huart, stm_log_level_t level);
void                 stm_log_set_level(stm_log_level_t level);

/* per-tag 级别（NONE = 删除该条目） */
void                 stm_log_set_tag_level(const char *tag, stm_log_level_t level);
stm_log_level_t      stm_log_get_tag_level(const char *tag);

/* 输出 callback（NULL = 恢复默认 UART） */
void                 stm_log_set_output(stm_log_output_fn output);

/* 日志宏 */
LOGV(tag, fmt, ...)                                     /* VERBOSE */
LOGD(tag, fmt, ...)                                     /* DEBUG   */
LOGI(tag, fmt, ...)                                     /* INFO    */
LOGW(tag, fmt, ...)                                     /* WARN    */
LOGE(tag, fmt, ...)                                     /* ERROR   */

/* HEX */
LOG_HEX(tag, buf, len)                                  /* INFO 级，16 字节/行 */
LOG_HEXD(tag, buf, len)                                 /* DEBUG 级，16 字节/行 */
```

## 用法示例

### 默认 UART 输出

```c
stm_log_init(&huart1, STM_LOG_LVL_INFO);
LOGI(TAG, "boot");
```

### 切到 RTT（J-Link + VSCode Cortex-Debug）

```c
#include "SEGGER_RTT.h"

static void rtt_output(const char *buf, uint16_t len) {
    SEGGER_RTT_Write(0, buf, len);
}

SEGGER_RTT_Init();
stm_log_init(&huart1, STM_LOG_LVL_INFO);               /* 默认 UART */
stm_log_set_output(rtt_output);                         /* 切到 RTT */
```

### 切到 SWO（ST-Link + VSCode Cortex-Debug）

```c
static void swo_output(const char *buf, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        ITM_SendChar((uint8_t)buf[i]);
    }
}

stm_log_init(&huart1, STM_LOG_LVL_INFO);
stm_log_set_output(swo_output);
```

> CubeMX 要配 PB3 = `SYS_SWO`，Debug 选 `Trace Asynchronous Sw`。

### HEX buffer

```c
uint8_t rx_buf[32] = { /* ... */ };

void on_rx_done(uint8_t *buf, uint16_t len) {
    LOG_HEXD("uart", buf, len);                         /* DEBUG 级 */
}
```

输出：

```
D (1234) uart: 0x[ 01 02 03 04 05 06 07 08  09 0a 0b 0c 0d 0e 0f 10 ]
D (1235) uart: 0x[ 11 12 13 ... ]
```

### 早期 log（`stm_log_init` 之前）

```c
int main(void) {
    HAL_Init();
    LOGI("boot", "step 1: HAL_Init done");              /* ← 进 ring buffer */

    SystemClock_Config();
    LOGI("boot", "step 2: clock=%lu MHz", SystemCoreClock);

    MX_USART1_UART_Init();

    stm_log_init(&huart1, STM_LOG_LVL_INFO);            /* ← 这一步自动 flush ring buffer → UART */
    LOGI("main", "step 3: log system up");
}
```

输出（无 log 丢失）：

```
I (1234) boot: step 1: HAL_Init done
I (1234) boot: step 2: clock=168 MHz
I (1234) main: step 3: log system up
```

### Release 关闭所有 log

```c
/* stm_log_config.h 或 CMake -D */
#define STM_LOG_ENABLED   0
```

LOGx 预处理后变 `do {} while (0)`，vsnprintf 调用 + 格式字符串全不进 binary。

## CMake 集成

### 方式 A：FetchContent（推荐，联网环境）

工程根 `CMakeLists.txt`：

```cmake
include(FetchContent)

FetchContent_Declare(
    stm_log
    GIT_REPOSITORY https://github.com/NingZiXi/stm_log.git
    GIT_TAG        v2.2.0
)
FetchContent_MakeAvailable(stm_log)

target_link_libraries(${YOUR_TARGET} PRIVATE stm_log)
target_link_libraries(${YOUR_TARGET} PRIVATE stm32cubemx)            /* 提供 HAL */
```

首次 build 自动 clone 到 `<build>/_deps/stm_log-src/`，版本锁定 `v2.2.0`。离线 / 代理环境不适用。

### 方式 B：手动 git clone（离线 / 代理环境）

```bash
mkdir -p Lib
git clone https://github.com/NingZiXi/stm_log Lib/stm_log
cd Lib/stm_log && git checkout v2.2.0
```

工程根 `CMakeLists.txt`：

```cmake
add_subdirectory(Lib/stm_log)

target_link_libraries(${YOUR_TARGET} PRIVATE stm_log)
target_link_libraries(${YOUR_TARGET} PRIVATE stm32cubemx)            /* 提供 HAL */
```

### Release build 关 log

```cmake
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(${YOUR_TARGET} PRIVATE STM_LOG_ENABLED=0)
endif()
```

## 编译期配置

`stm_log_config.h`：

| 宏 | 默认 | 含义 |
|---|---|---|
| `STM_LOG_ENABLED`             | 1                    | 0: 所有 LOGx 空宏（Release 用） |
| `STM_LOG_BUFFER_SIZE`         | 128                  | 单条 log 最大字节（vsnprintf 截断） |
| `STM_LOG_USE_COLORS`          | 1                    | 1: ANSI 颜色；0: 关闭 |
| `STM_LOG_LEVEL_DEFAULT`       | `STM_LOG_LVL_INFO`   | stm_log_init 之前的默认级别 |
| `STM_LOG_MAX_TAGS`            | 16                   | per-tag 级别表最大条目数 |
| `STM_LOG_INCLUDE_FILE_LINE`   | 0                    | 1: LOGx 加 `[file:line]`；0: 关闭 |
| `STM_LOG_EARLY_BUFFER_SIZE`   | 1024                 | 早期 log ring buffer 字节数；0 = 禁用 |
| `STM_LOG_USE_MUTEX`           | 0                    | 1: FreeRTOS recursive mutex 保护共享状态（多任务）；0: 裸机 / 单任务 |

## 约束

- **不能在中断服务例程里调 LOGx**（v2.x 阻塞输出）
- **多任务并发调 LOGx**：`STM_LOG_USE_MUTEX=1` 时安全；写操作（set_*）加锁，读路径（LOGx / get_tag_level）不加锁以保性能，容忍极小概率读到 s_tags 撕裂值（漏一条 log，不 crash）
- **STM_LOG_USE_MUTEX=1**：`stm_log_init()` 必须在 FreeRTOS scheduler 启动后调用（mutex 用 heap 分配）
- 单 UART 默认绑定；通过 `stm_log_set_output()` 运行时切换
- per-tag 表大小由 `STM_LOG_MAX_TAGS` 编译期固定
- 早期 log ring buffer 由 `STM_LOG_EARLY_BUFFER_SIZE` 编译期固定
- 依赖 STM32 HAL（`stm32f4xx_hal.h` / `HAL_UART_Transmit` / `HAL_GetTick`）

## License

MIT