/** @file test_log.c
 *  @brief 真实日志实现的输出、过滤、边界与开关测试。
 */
#include "stm_log.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static char captured[8192];
static size_t used;
static unsigned calls;
static void output(const char *data, uint16_t len)
{
    CHECK(data && len && len <= STM_LOG_EARLY_BUFFER_SIZE + STM_LOG_BUFFER_SIZE + 2U);
    calls++;
    if (used + len < sizeof captured) {
        memcpy(captured + used, data, len);
        used += len;
        captured[used] = 0;
    }
}
static void clear(void) { used = 0; captured[0] = 0; calls = 0; }
static uint32_t tick(void) { return 1234U; }

int main(void)
{
    stm_log_set_tick(tick);
    LOGI("e", "one");
    LOGI("e", "two");
    stm_log_init_output(NULL, STM_LOG_LVL_INFO);
    CHECK(calls == 0);
    stm_log_init_output(output, STM_LOG_LVL_INFO);
#if STM_LOG_ENABLED && STM_LOG_EARLY_BUFFER_SIZE > 0
    CHECK(strstr(captured, "one") && strstr(captured, "two"));
#if STM_LOG_AUTO_NEWLINE
    CHECK(strstr(captured, "\r\n"));
#endif
#else
    CHECK(calls == 0);
#endif
    clear();
#if !STM_LOG_ENABLED
    int evaluated = 0;
    LOGI("off", "%d", ++evaluated);
    stm_log(STM_LOG_LVL_INFO, "off", "direct");
    const uint8_t data = 1;
    stm_log_hex(STM_LOG_LVL_INFO, "off", &data, 1, 1);
    CHECK(evaluated == 0 && calls == 0);
#else
    // 不经宏测试短输出，避免小缓冲被 file:line 占满。
    stm_log(STM_LOG_LVL_INFO, "t", "ok");
    CHECK(strstr(captured, "1234") && strstr(captured, "ok"));
#if STM_LOG_AUTO_NEWLINE
    CHECK(used >= 2 && !memcmp(captured + used - 2, "\r\n", 2));
#endif
    clear();
    stm_log_set_tick(NULL);
    stm_log(STM_LOG_LVL_INFO, "t", "zero");
    CHECK(strstr(captured, "(0)"));
    clear();
    stm_log_set_tag_level("at_comms", STM_LOG_LVL_NONE);
    LOGE("at_comms", "quiet");
    CHECK(calls == 0);
    stm_log_set_tag_level("at_comms", STM_LOG_LVL_VERBOSE);
    LOGV("at_comms", "AT");
    CHECK(calls == 1);
    stm_log_unset_tag_level("at_comms");
    CHECK(stm_log_get_tag_level("at_comms") == STM_LOG_LVL_INFO);
#if STM_LOG_BUFFER_SIZE >= 96
    clear();
    stm_log(STM_LOG_LVL_INFO, "format", "u32=%u s32=%d x32=%X",
            (unsigned)UINT32_MAX, (int)INT32_MIN, (unsigned)UINT32_MAX);
    CHECK(strstr(captured, "u32=4294967295 s32=-2147483648 x32=FFFFFFFF"));
    clear();
    stm_log(STM_LOG_LVL_INFO, "format", "u64=%llu s64=%lld x64=%llX",
            (unsigned long long)UINT64_MAX, (long long)INT64_MIN,
            (unsigned long long)UINT64_MAX);
    CHECK(strstr(captured,
          "u64=18446744073709551615 s64=-9223372036854775808 x64=FFFFFFFFFFFFFFFF"));
    clear();
#endif
    clear(); // Isolate long-message assertions from the tag-level test output.
    char long_text[1024]; memset(long_text, 'x', sizeof long_text - 1); long_text[1023] = 0;
    stm_log(STM_LOG_LVL_INFO, long_text, "%s", long_text);
    CHECK(calls == 1 && used <= STM_LOG_BUFFER_SIZE + 1U);
    clear();
    stm_log((stm_log_level_t)-2, "x", "invalid");
    stm_log((stm_log_level_t)99, "x", "invalid");
    stm_log(STM_LOG_LVL_INFO, NULL, NULL);
    stm_log_hex(STM_LOG_LVL_INFO, "x", NULL, 1, 16);
    CHECK(calls == 0);
    static uint8_t large[65535];
    stm_log_hex(STM_LOG_LVL_INFO, "h", large, sizeof large, 32768);
    CHECK(calls == 2); // 末轮偏移不得回绕，超长行安全截断。
    clear();
    stm_log_set_output(NULL);
    stm_log(STM_LOG_LVL_INFO, "t", "hold");
    CHECK(calls == 0);
    stm_log_set_output(output);
#if STM_LOG_EARLY_BUFFER_SIZE > 0
    CHECK(strstr(captured, "hold"));
#else
    CHECK(calls == 0);
#endif
#if STM_LOG_INCLUDE_FILE_LINE
    clear();
    stm_log_fl(STM_LOG_LVL_INFO, "a/b.c", 42, "t", "file");
    CHECK(strstr(captured, "b.c:42"));
#endif
#endif
    puts("stm_log checks PASS");
    return 0;
}
