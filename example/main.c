/** @file main.c
 *  @brief 无 MCU SDK 的最小日志示例；嵌入式工程只替换板级输出与时钟。
 */
#include "stm_log.h"
#include <stdio.h>
#include <time.h>

static void console_output(const char *data, uint16_t len)
{
    (void)fwrite(data, 1U, len, stdout); // 必须按 len 发送，不能假设有 NUL。
}

static uint32_t board_millis(void)
{
    // 主机示例使用进程 CPU 时间；设备上替换为自己的单调毫秒计数。
    return (uint32_t)((double)clock() * 1000.0 / CLOCKS_PER_SEC);
}

static void board_init(void)
{
    // MCU 工程在这里初始化外设，再注入 UART、RTT 或其他输出回调。
    stm_log_set_tick(board_millis);
    stm_log_init_output(console_output, STM_LOG_LVL_INFO);
}

int main(void)
{
    LOGI("boot", "early log: no output bound yet");
    board_init(); // 同时刷出早期日志。
    LOGI("app", "version %s", "1.0.0");
    stm_log_set_tag_level("at_comms", STM_LOG_LVL_VERBOSE);
    LOGV("at_comms", "<< AT");
    LOGV("at_comms", ">> OK");
    const uint8_t data[] = {0x01, 0x02, 0xA5, 0xFF};
    LOG_HEX("rx", data, sizeof data);
    stm_log_set_tag_level("at_comms", STM_LOG_LVL_NONE);
    LOGV("at_comms", "this message is muted");
    stm_log_unset_tag_level("at_comms");
    stm_log_set_output(NULL); // 暂停输出；不恢复任何隐式 UART 后端。
    LOGW("app", "buffered until output returns");
    stm_log_set_output(console_output);
    return 0;
}
