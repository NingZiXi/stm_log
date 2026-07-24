/**
 * @file    stm_log.h
 * @author  宁子希 (1589326497@qq.com)
 * @brief   STM32 HAL 专用分级日志组件 — 5 级 / per-tag / 自定义输出 / HEX / 早期 log
 * @date    2026-07-18
 * @version 2.3.0
 *
 * @copyright Copyright (c) 2026
 *
 * @details 专为 STM32 HAL UART 优化：
 *          - 默认阻塞 UART 输出
 *          - 运行时可切到 RTT / SWO / 自定义后端
 *          - 支持编译期全关（STM_LOG_ENABLED=0）以用于量产 release 固件
 *
 *   HAL 头文件由使用方通过 STM_LOG_HAL_HEADER 指定（默认 stm32f4xx_hal.h），
 *   其它 STM32 家族通过编译选项覆盖。详见 README.md「编译期配置」。
 *
 *   用法见 README.md，编译期选项见 stm_log_config.h。
 */

#ifndef STM_LOG_H
#define STM_LOG_H

#include <stdint.h>
#include <stdarg.h>

#ifndef STM_LOG_HAL_HEADER
#define STM_LOG_HAL_HEADER "stm32f4xx_hal.h"                          /*!< HAL 头文件，默认兼容 STM32F4 */
#endif
#include STM_LOG_HAL_HEADER

#include "stm_log_config.h"

#ifdef STM_LOG_INCLUDE_FILE_LINE
#include <string.h>                                                   /*!< stm_log_basename() 用 strrchr */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ─── 公共类型 ─── */

/**
 * @brief 日志级别（数值越大越详细）
 */
typedef enum {
    STM_LOG_LVL_NONE    = -1,                                         /*!< 完全关闭 */
    STM_LOG_LVL_ERROR   =  0,
    STM_LOG_LVL_WARN    =  1,
    STM_LOG_LVL_INFO    =  2,
    STM_LOG_LVL_DEBUG   =  3,
    STM_LOG_LVL_VERBOSE =  4,
} stm_log_level_t;

/**
 * @brief per-tag 级别表条目
 */
typedef struct {
    const char      *tag;
    stm_log_level_t  level;
} stm_log_tag_cfg_t;

/**
 * @brief 自定义输出回调签名 — 用户实现后通过 stm_log_set_output() 注入
 *
 * @param  buf  已格式化好的整条 log 字符串（不含 \\r\\n，由 callback 自行决定）
 * @param  len  buf 长度
 */
typedef void (*stm_log_output_fn)(const char *buf, uint16_t len);

/* ─── 公共 API ─── */

/**
 * @brief 初始化日志组件（默认 UART 输出）
 *
 * @param  huart  指向已初始化的 UART_HandleTypeDef（如 &huart1）；可传 NULL 跳过 UART 绑定
 * @param  level  全局默认级别（未注册的 tag 走此 level）
 *
 * @note  v2.3.0 起 `huart` 允许 NULL：RTT/SWO/USB CDC 等非 UART 后端可传 NULL 避免虚假 UART 依赖。
 *        推荐非 UART 后端改用 `stm_log_init_output()`，语义更清晰。
 */
void stm_log_init(UART_HandleTypeDef *huart, stm_log_level_t level);

/**
 * @brief 一步完成 init + set_output：设全局级别 + 装自定义 callback + flush 早期 ring buffer
 *
 * @param  output  自定义输出 callback（NULL = 恢复默认 UART 输出）
 * @param  level   全局默认级别
 *
 * @note  推荐用于 RTT / SWO / USB CDC 等非 UART 后端。
 *        与 `stm_log_init()` 等价但跳过 UART 绑定；与 `stm_log_set_output()` 等价但同时设 level + flush。
 */
void stm_log_init_output(stm_log_output_fn output, stm_log_level_t level);

/**
 * @brief 切换全局默认级别（不影响 per-tag 设置）
 */
void stm_log_set_level(stm_log_level_t level);

/**
 * @brief 注册 / 更新某 tag 的级别（STM_LOG_LVL_NONE = 静音该 tag 所有输出）
 */
void stm_log_set_tag_level(const char *tag, stm_log_level_t level);

/**
 * @brief 删除某 tag 的 per-tag 配置（让该 tag 回退到全局默认）
 */
void stm_log_unset_tag_level(const char *tag);

/**
 * @brief 查询某 tag 当前生效级别
 *
 * @return 当前生效级别
 */
stm_log_level_t stm_log_get_tag_level(const char *tag);

/**
 * @brief 运行时切换输出 callback（NULL = 恢复默认 UART）
 *
 * @code
 *   static void rtt_output(const char *buf, uint16_t len) {
 *       SEGGER_RTT_Write(0, buf, len);
 *   }
 *   stm_log_set_output(rtt_output);
 * @endcode
 */
void stm_log_set_output(stm_log_output_fn output);

/**
 * @brief 通用日志输出入口（LOGx 宏内部调用）
 */
void stm_log(stm_log_level_t level, const char *tag, const char *fmt, ...);

#if STM_LOG_INCLUDE_FILE_LINE
/**
 * @brief 带 file:line 的日志输出入口（STM_LOG_INCLUDE_FILE_LINE=1 时启用）
 */
void stm_log_fl(stm_log_level_t level, const char *file, int line,
                const char *tag, const char *fmt, ...);
#endif

/**
 * @brief hex buffer 打印入口（LOG_HEX/LOG_HEXD 宏内部调用）
 *
 * @param  level           输出级别
 * @param  tag             模块标签
 * @param  buf             数据缓冲区
 * @param  len             数据长度
 * @param  bytes_per_line  每行多少字节（典型 16）
 */
void stm_log_hex(stm_log_level_t level, const char *tag,
                 const void *buf, uint16_t len, uint16_t bytes_per_line);

/* ─── 编译期三路分支（STM_LOG_ENABLED × STM_LOG_INCLUDE_FILE_LINE） ─── */

#if !STM_LOG_ENABLED

/* 全局关闭：所有 LOGx 预处理阶段变空宏（vsnprintf 调用和字符串全不进 binary） */
#define LOGD(tag, fmt, ...)           do {} while (0)
#define LOGI(tag, fmt, ...)           do {} while (0)
#define LOGW(tag, fmt, ...)           do {} while (0)
#define LOGE(tag, fmt, ...)           do {} while (0)
#define LOGV(tag, fmt, ...)           do {} while (0)
#define LOG_HEX(tag, buf, len)        do {} while (0)
#define LOG_HEXD(tag, buf, len)       do {} while (0)

#elif STM_LOG_INCLUDE_FILE_LINE

#define LOGD(tag, fmt, ...)  stm_log_fl(STM_LOG_LVL_DEBUG,   __FILE__, __LINE__, tag, fmt, ##__VA_ARGS__)
#define LOGI(tag, fmt, ...)  stm_log_fl(STM_LOG_LVL_INFO,    __FILE__, __LINE__, tag, fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...)  stm_log_fl(STM_LOG_LVL_WARN,    __FILE__, __LINE__, tag, fmt, ##__VA_ARGS__)
#define LOGE(tag, fmt, ...)  stm_log_fl(STM_LOG_LVL_ERROR,   __FILE__, __LINE__, tag, fmt, ##__VA_ARGS__)
#define LOGV(tag, fmt, ...)  stm_log_fl(STM_LOG_LVL_VERBOSE, __FILE__, __LINE__, tag, fmt, ##__VA_ARGS__)
#define LOG_HEX(tag, buf, len)    stm_log_hex(STM_LOG_LVL_INFO,  tag, buf, len, 16)
#define LOG_HEXD(tag, buf, len)   stm_log_hex(STM_LOG_LVL_DEBUG, tag, buf, len, 16)

#else

#define LOGD(tag, fmt, ...)  stm_log(STM_LOG_LVL_DEBUG,   tag, fmt, ##__VA_ARGS__)
#define LOGI(tag, fmt, ...)  stm_log(STM_LOG_LVL_INFO,    tag, fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...)  stm_log(STM_LOG_LVL_WARN,    tag, fmt, ##__VA_ARGS__)
#define LOGE(tag, fmt, ...)  stm_log(STM_LOG_LVL_ERROR,   tag, fmt, ##__VA_ARGS__)
#define LOGV(tag, fmt, ...)  stm_log(STM_LOG_LVL_VERBOSE, tag, fmt, ##__VA_ARGS__)
#define LOG_HEX(tag, buf, len)    stm_log_hex(STM_LOG_LVL_INFO,  tag, buf, len, 16)
#define LOG_HEXD(tag, buf, len)   stm_log_hex(STM_LOG_LVL_DEBUG, tag, buf, len, 16)

#endif

#ifdef __cplusplus
}
#endif

#endif /* STM_LOG_H */