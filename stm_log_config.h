/**
 * @file    stm_log_config.h
 * @author  宁子希 (1589326497@qq.com)
 * @brief   stm_log 编译期配置
 * @date    2026-07-18
 * @version 2.1.0
 *
 * @copyright Copyright (c) 2026
 *
 * 改这里后必须重新编译；Release build 推荐改 STM_LOG_ENABLED=0。
 */

#ifndef STM_LOG_CONFIG_H
#define STM_LOG_CONFIG_H

#define STM_LOG_ENABLED             1                                  /*!< 1: LOGx 正常工作；0: 所有 LOGx → do{}while(0)（Release / 量产用） */
#define STM_LOG_BUFFER_SIZE         128                                /*!< 单条日志最大字节数（vsnprintf 截断） */
#define STM_LOG_USE_COLORS          1                                  /*!< 1: ANSI 颜色（红/黄/绿/灰/青）；0: 关闭 */
#define STM_LOG_LEVEL_DEFAULT       STM_LOG_LVL_INFO                   /*!< stm_log_init 之前 LOGx 默认级别 */
#define STM_LOG_MAX_TAGS            16                                 /*!< per-tag 级别表最大条目（运行时注册） */
#define STM_LOG_INCLUDE_FILE_LINE   0                                  /*!< 1: LOGx 自动加 __FILE__/__LINE__，输出 [file:line]；0: 关闭 */
#define STM_LOG_EARLY_BUFFER_SIZE   1024                               /*!< 早期 log ring buffer 字节数（stm_log_init 前暂存）；
                                                                          0 = 禁用早期 log（stm_log_init 前 LOGx 直接丢弃） */
#define STM_LOG_USE_MUTEX           0                                  /*!< 1: 用 FreeRTOS recursive mutex 保护共享状态（多任务项目）；
                                                                          0: 裸机 / 单任务（零开销） */

#endif /* STM_LOG_CONFIG_H */