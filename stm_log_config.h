/** @file stm_log_config.h
 *  @brief 日志默认配置；可通过编译宏覆盖，所有调用方须保持一致。
 *  @copyright Copyright (c) 2026
 */
#ifndef STM_LOG_CONFIG_H
#define STM_LOG_CONFIG_H
#ifndef STM_LOG_ENABLED
#define STM_LOG_ENABLED 1
#endif
#ifndef STM_LOG_BUFFER_SIZE
#define STM_LOG_BUFFER_SIZE 128
#endif
#ifndef STM_LOG_USE_COLORS
#define STM_LOG_USE_COLORS 1
#endif
#ifndef STM_LOG_LEVEL_DEFAULT
#define STM_LOG_LEVEL_DEFAULT STM_LOG_LVL_INFO
#endif
#ifndef STM_LOG_MAX_TAGS
#define STM_LOG_MAX_TAGS 16
#endif
#ifndef STM_LOG_INCLUDE_FILE_LINE
#define STM_LOG_INCLUDE_FILE_LINE 0
#endif
#ifndef STM_LOG_EARLY_BUFFER_SIZE
#define STM_LOG_EARLY_BUFFER_SIZE 1024
#endif
// 兼容旧的可选 FreeRTOS 配置；默认关闭，不引入 RTOS 或动态分配。
#ifndef STM_LOG_USE_MUTEX
#define STM_LOG_USE_MUTEX 0
#endif
#ifndef STM_LOG_AUTO_NEWLINE
#define STM_LOG_AUTO_NEWLINE 1
#endif
#endif
