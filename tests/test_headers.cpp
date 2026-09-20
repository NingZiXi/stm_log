/** @file test_headers.cpp
 *  @brief 不包含 HAL 的 C++ 头文件与链接检查。
 */
#include "stm_log.h"
static void output(const char *, uint16_t) {}
static uint32_t tick() { return 123; }
int main()
{
    stm_log_set_tick(tick);
    stm_log_init_output(output, STM_LOG_LVL_INFO);
    LOGI("cpp", "linked");
    return stm_log_get_tag_level("cpp") == STM_LOG_LVL_INFO ? 0 : 1;
}
