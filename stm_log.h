/** @file stm_log.h
 *  @brief 平台无关日志：分级、tag、输出回调、时间戳和 HEX。
 *  @copyright Copyright (c) 2026
 */
#ifndef STM_LOG_H
#define STM_LOG_H

#include <stdint.h>
#include <stdarg.h>


#include "stm_log_config.h"


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
 * @param  buf  日志字节；按 STM_LOG_AUTO_NEWLINE 决定是否包含 CRLF，不保证 NUL 结尾。
 * @param  len  有效字节数；回调必须在返回前发送完成或复制，不得保存栈指针。
 */
typedef void (*stm_log_output_fn)(const char *buf, uint16_t len);

/* ─── 公共 API ─── */

/**
 * @brief 一步完成 init + set_output：设全局级别 + 装自定义 callback + flush 早期 ring buffer
 *
 * @param  output  自定义输出 callback（NULL = 暂停输出，后续日志按早期缓冲策略处理）
 * @param  level   全局默认级别
 *
 * @note UART、RTT、SWO 使用同一个回调接口；不初始化外设，不内置默认后端。
 *       只在单调用者任务/主循环使用；不可在 ISR 或输出/时钟回调重入。
 */
void stm_log_init_output(stm_log_output_fn output, stm_log_level_t level);

/** 毫秒时钟回调；必须立即返回，不得重入日志。允许 uint32_t 回绕。 */
typedef uint32_t (*stm_log_tick_fn)(void);
/** 设置时间来源；NULL 表示时间戳为 0。建议在 init_output 前设置。 */
void stm_log_set_tick(stm_log_tick_fn tick);

/**
 * @brief 切换全局默认级别（不影响 per-tag 设置）
 */
void stm_log_set_level(stm_log_level_t level);

/**
 * @brief 注册 / 更新某 tag 的级别（STM_LOG_LVL_NONE = 静音该 tag 所有输出）
 * @note tag 字符串须保持有效至 unset；表满时忽略新增项，已有项可更新。
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
 * @brief 运行时切换输出 callback（NULL = 暂停输出）
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
