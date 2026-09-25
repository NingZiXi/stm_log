/** @file stm_log.c
 *  @brief 平台无关日志实现；外设输出与毫秒时钟由应用注入。
 *  @copyright Copyright (c) 2026
 */
#include "stm_log.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>

_Static_assert(STM_LOG_BUFFER_SIZE >= 16 && STM_LOG_BUFFER_SIZE <= 65533, "invalid log buffer size");
_Static_assert(STM_LOG_EARLY_BUFFER_SIZE >= 0 && STM_LOG_EARLY_BUFFER_SIZE <= 65535, "invalid early buffer size");
_Static_assert(STM_LOG_MAX_TAGS > 0 && STM_LOG_MAX_TAGS <= 255, "invalid tag count");

/* ─── FreeRTOS mutex（多任务保护，可选） ─── */
#if STM_LOG_USE_MUTEX
#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t s_mutex;

#define LOCK()   do { if (s_mutex) (void)xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY); } while (0)
#define UNLOCK() do { if (s_mutex) (void)xSemaphoreGiveRecursive(s_mutex);                   } while (0)
#else
#define LOCK()   do {} while (0)
#define UNLOCK() do {} while (0)
#endif

/* ─── 全局状态 ─── */

static stm_log_output_fn   s_output;                                  /*!< 当前输出 callback */
static stm_log_tick_fn s_tick;
static stm_log_level_t     s_level = STM_LOG_LEVEL_DEFAULT;           /*!< 全局默认过滤级别 */

static stm_log_tag_cfg_t   s_tags[STM_LOG_MAX_TAGS];                  /*!< per-tag 级别表 */
static uint8_t             s_tag_count;                               /*!< 已注册 tag 数 */

#if STM_LOG_EARLY_BUFFER_SIZE > 0
static char     s_early_buf[STM_LOG_EARLY_BUFFER_SIZE];               /*!< 早期 log ring buffer */
static uint16_t s_early_pos;                                          /*!< 已写入字节数 */
#endif

#if STM_LOG_USE_COLORS
static const char *const s_lvl_color[] = {
    "\033[31m",                                                       /*!< ERROR  ：红 */
    "\033[33m",                                                       /*!< WARN   ：黄 */
    "\033[32m",                                                       /*!< INFO   ：绿 */
    "\033[90m",                                                       /*!< DEBUG  ：灰 */
    "\033[96m",                                                       /*!< VERBOSE：青 */
};
#endif
static const char s_lvl_chr[] = { 'E', 'W', 'I', 'D', 'V' };          /*!< 各级别单字符前缀 */

/* ─── 内部 helper ─── */

static uint32_t log_tick(void) { return s_tick ? s_tick() : 0U; }
void stm_log_set_tick(stm_log_tick_fn tick) { s_tick = tick; }

/**
 * @brief 通用内部输出 — 单一入口，所有 LOGx 路径走这里
 */
static inline void emit(const char *buf, uint16_t len) {
    if (s_output) {
#if STM_LOG_AUTO_NEWLINE
        char combined[STM_LOG_BUFFER_SIZE + 2];
        if (len <= STM_LOG_BUFFER_SIZE) {
            memcpy(combined, buf, len);
            combined[len]     = '\r';
            combined[len + 1] = '\n';
            s_output(combined, (uint16_t)(len + 2));
            return;
        }
#endif
        s_output(buf, len);
#if STM_LOG_AUTO_NEWLINE
        static const char s_nl[] = "\r\n";
        s_output(s_nl, sizeof(s_nl) - 1);
#endif
    }
}

/**
 * @brief 早期 log 写入 ring buffer（满则静默丢弃，避免覆盖早期更重要的 log）
 */
static void early_write(const char *buf, uint16_t len) {
#if STM_LOG_EARLY_BUFFER_SIZE > 0
    uint16_t free_space = (uint16_t)(sizeof(s_early_buf) - s_early_pos);
    const uint16_t newline = STM_LOG_AUTO_NEWLINE ? 2U : 0U;
    if ((uint32_t)len + newline <= free_space) {
        memcpy(s_early_buf + s_early_pos, buf, len);
        s_early_pos = (uint16_t)(s_early_pos + len);
        if (newline) {
            s_early_buf[s_early_pos++] = '\r';
            s_early_buf[s_early_pos++] = '\n';
        }
    }
#else
    (void)buf; (void)len;
#endif
}

/**
 * @brief 早期 log 整体 flush 到当前 callback
 */
static void early_flush(void) {
#if STM_LOG_EARLY_BUFFER_SIZE > 0
    if (s_output && s_early_pos > 0) {
        s_output(s_early_buf, s_early_pos);
        s_early_pos = 0;
    }
#endif
}

/**
 * @brief 查表 — 已知 tag 优先，否则回退全局默认
 *
 * @note  读路径不加锁；应用必须串行化日志和配置调用。
 */
static stm_log_level_t resolve_level(const char *tag) {
    if (tag) {
        for (uint8_t i = 0; i < s_tag_count; i++) {
            if (strcmp(s_tags[i].tag, tag) == 0) {
                return s_tags[i].level;
            }
        }
    }
    return s_level;
}

typedef struct {
    char *buf;
    size_t cap;
    size_t total;
} stm_fmt_out_t;

typedef enum {
    STM_LEN_NONE,
    STM_LEN_HH,
    STM_LEN_H,
    STM_LEN_L,
    STM_LEN_LL,
    STM_LEN_J,
    STM_LEN_Z,
    STM_LEN_T,
    STM_LEN_CAP_L,
} stm_fmt_len_t;

static void fmt_putc(stm_fmt_out_t *out, char ch)
{
    if (out->cap > 0U && out->total + 1U < out->cap) {
        out->buf[out->total] = ch;
    }
    out->total++;
}

static void fmt_repeat(stm_fmt_out_t *out, char ch, size_t count)
{
    while (count-- > 0U) fmt_putc(out, ch);
}

static void fmt_write(stm_fmt_out_t *out, const char *text, size_t len)
{
    for (size_t i = 0; i < len; ++i) fmt_putc(out, text[i]);
}

static uint64_t fmt_unsigned_arg(va_list *ap, stm_fmt_len_t len)
{
    switch (len) {
    case STM_LEN_HH: return (unsigned char)va_arg(*ap, unsigned int);
    case STM_LEN_H:  return (unsigned short)va_arg(*ap, unsigned int);
    case STM_LEN_L:  return va_arg(*ap, unsigned long);
    case STM_LEN_LL: return va_arg(*ap, unsigned long long);
    case STM_LEN_J:  return va_arg(*ap, uintmax_t);
    case STM_LEN_Z:  return va_arg(*ap, size_t);
    case STM_LEN_T:  return (uint64_t)va_arg(*ap, ptrdiff_t);
    default:         return va_arg(*ap, unsigned int);
    }
}

static int64_t fmt_signed_arg(va_list *ap, stm_fmt_len_t len)
{
    switch (len) {
    case STM_LEN_HH: return (signed char)va_arg(*ap, int);
    case STM_LEN_H:  return (short)va_arg(*ap, int);
    case STM_LEN_L:  return va_arg(*ap, long);
    case STM_LEN_LL: return va_arg(*ap, long long);
    case STM_LEN_J:  return va_arg(*ap, intmax_t);
    case STM_LEN_Z:  return (int64_t)va_arg(*ap, ptrdiff_t);
    case STM_LEN_T:  return (int64_t)va_arg(*ap, ptrdiff_t);
    default:         return va_arg(*ap, int);
    }
}

static void fmt_integer(stm_fmt_out_t *out, uint64_t value, unsigned base,
                        int uppercase, int negative, int show_plus,
                        int space_sign, int alternate, int left, int zero,
                        int width, int precision)
{
    char digits[64];
    const char *alphabet = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t ndigits = 0U;
    if (value != 0U || precision != 0) {
        do {
            digits[ndigits++] = alphabet[value % base];
            value /= base;
        } while (value != 0U);
    }

    char sign = negative ? '-' : (show_plus ? '+' : (space_sign ? ' ' : '\0'));
    char prefix[2];
    size_t prefix_len = 0U;
    if (alternate && base == 16U && ndigits > 0U) {
        prefix[0] = '0'; prefix[1] = uppercase ? 'X' : 'x'; prefix_len = 2U;
    } else if (alternate && base == 8U && (ndigits == 0U || digits[ndigits - 1U] != '0')) {
        prefix[0] = '0'; prefix_len = 1U;
    }

    size_t leading_zeroes = 0U;
    if (precision >= 0 && (size_t)precision > ndigits) leading_zeroes = (size_t)precision - ndigits;
    size_t core = (sign ? 1U : 0U) + prefix_len + leading_zeroes + ndigits;
    size_t padding = width > 0 && (size_t)width > core ? (size_t)width - core : 0U;
    if (zero && !left && precision < 0) {
        leading_zeroes += padding;
        padding = 0U;
    }
    if (!left) fmt_repeat(out, ' ', padding);
    if (sign) fmt_putc(out, sign);
    fmt_write(out, prefix, prefix_len);
    fmt_repeat(out, '0', leading_zeroes);
    while (ndigits > 0U) fmt_putc(out, digits[--ndigits]);
    if (left) fmt_repeat(out, ' ', padding);
}

/*
 * Small integer-capable formatter used instead of relying on newlib-nano's
 * optional long-long printf support. Floating-point conversions are delegated
 * one conversion at a time to the platform snprintf, preserving existing API
 * behaviour when the target C library enables them.
 */
static int stm_vsnprintf(char *buf, size_t cap, const char *fmt, va_list input)
{
    stm_fmt_out_t out = { buf, cap, 0U };
    va_list ap;
    va_copy(ap, input);

    while (*fmt) {
        if (*fmt != '%') {
            fmt_putc(&out, *fmt++);
            continue;
        }
        ++fmt;
        if (*fmt == '%') {
            fmt_putc(&out, *fmt++);
            continue;
        }

        int left = 0, show_plus = 0, space_sign = 0, alternate = 0, zero = 0;
        for (;;) {
            if (*fmt == '-') left = 1;
            else if (*fmt == '+') show_plus = 1;
            else if (*fmt == ' ') space_sign = 1;
            else if (*fmt == '#') alternate = 1;
            else if (*fmt == '0') zero = 1;
            else break;
            ++fmt;
        }

        int width = -1;
        if (*fmt == '*') {
            width = va_arg(ap, int);
            ++fmt;
            if (width < 0) { left = 1; width = -width; }
        } else if (isdigit((unsigned char)*fmt)) {
            width = 0;
            while (isdigit((unsigned char)*fmt)) width = width * 10 + (*fmt++ - '0');
        }

        int precision = -1;
        if (*fmt == '.') {
            ++fmt;
            if (*fmt == '*') {
                precision = va_arg(ap, int);
                ++fmt;
                if (precision < 0) precision = -1;
            } else {
                precision = 0;
                while (isdigit((unsigned char)*fmt)) precision = precision * 10 + (*fmt++ - '0');
            }
        }

        stm_fmt_len_t len = STM_LEN_NONE;
        if (fmt[0] == 'h' && fmt[1] == 'h') { len = STM_LEN_HH; fmt += 2; }
        else if (*fmt == 'h') { len = STM_LEN_H; ++fmt; }
        else if (fmt[0] == 'l' && fmt[1] == 'l') { len = STM_LEN_LL; fmt += 2; }
        else if (*fmt == 'l') { len = STM_LEN_L; ++fmt; }
        else if (*fmt == 'j') { len = STM_LEN_J; ++fmt; }
        else if (*fmt == 'z') { len = STM_LEN_Z; ++fmt; }
        else if (*fmt == 't') { len = STM_LEN_T; ++fmt; }
        else if (*fmt == 'L') { len = STM_LEN_CAP_L; ++fmt; }

        char conv = *fmt ? *fmt++ : '\0';
        switch (conv) {
        case 'd':
        case 'i': {
            int64_t sval = fmt_signed_arg(&ap, len);
            uint64_t magnitude = sval < 0 ? 0U - (uint64_t)sval : (uint64_t)sval;
            fmt_integer(&out, magnitude, 10U, 0, sval < 0, show_plus, space_sign,
                        0, left, zero, width, precision);
            break;
        }
        case 'u': case 'o': case 'x': case 'X': {
            unsigned base = conv == 'o' ? 8U : ((conv == 'x' || conv == 'X') ? 16U : 10U);
            fmt_integer(&out, fmt_unsigned_arg(&ap, len), base, conv == 'X', 0, 0, 0,
                        alternate, left, zero, width, precision);
            break;
        }
        case 'p': {
            uintptr_t value = (uintptr_t)va_arg(ap, void *);
            fmt_integer(&out, (uint64_t)value, 16U, 0, 0, 0, 0, 1,
                        left, zero, width, precision);
            break;
        }
        case 'c': {
            char ch = (char)va_arg(ap, int);
            size_t padding = width > 1 ? (size_t)width - 1U : 0U;
            if (!left) fmt_repeat(&out, ' ', padding);
            fmt_putc(&out, ch);
            if (left) fmt_repeat(&out, ' ', padding);
            break;
        }
        case 's': {
            const char *text = va_arg(ap, const char *);
            if (!text) text = "(null)";
            size_t n = strlen(text);
            if (precision >= 0 && (size_t)precision < n) n = (size_t)precision;
            size_t padding = width > 0 && (size_t)width > n ? (size_t)width - n : 0U;
            if (!left) fmt_repeat(&out, ' ', padding);
            fmt_write(&out, text, n);
            if (left) fmt_repeat(&out, ' ', padding);
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
            char spec[48];
            char temp[128];
            size_t pos = 0U;
            spec[pos++] = '%';
            if (left) spec[pos++] = '-';
            if (show_plus) spec[pos++] = '+';
            if (space_sign) spec[pos++] = ' ';
            if (alternate) spec[pos++] = '#';
            if (zero) spec[pos++] = '0';
            if (width >= 0) pos += (size_t)snprintf(spec + pos, sizeof(spec) - pos, "%d", width);
            if (precision >= 0) pos += (size_t)snprintf(spec + pos, sizeof(spec) - pos, ".%d", precision);
            if (len == STM_LEN_CAP_L) spec[pos++] = 'L';
            spec[pos++] = conv; spec[pos] = '\0';
            int n = len == STM_LEN_CAP_L
                ? snprintf(temp, sizeof(temp), spec, va_arg(ap, long double))
                : snprintf(temp, sizeof(temp), spec, va_arg(ap, double));
            if (n > 0) {
                size_t actual = (size_t)n < sizeof(temp) ? (size_t)n : sizeof(temp) - 1U;
                fmt_write(&out, temp, actual);
            }
            break;
        }
        case '\0':
            fmt_putc(&out, '%');
            goto done;
        default:
            /* Keep unsupported conversions visible without consuming an argument. */
            fmt_putc(&out, '%');
            fmt_putc(&out, conv);
            break;
        }
    }

done:
    va_end(ap);
    if (cap > 0U) {
        size_t end = out.total < cap - 1U ? out.total : cap - 1U;
        buf[end] = '\0';
    }
    return out.total > (size_t)INT32_MAX ? INT32_MAX : (int)out.total;
}

/**
 * @brief snprintf + 截断处理，统一添加 level / tag / 可选 [file:line] 前缀
 *
 * @return 最终字节数（<= buf_size - 1）；负值 = 编码错误
 */
// snprintf 返回“需要的长度”，不能直接用于下一次指针偏移。
static void append_format(char *buf, size_t cap, size_t *used, const char *fmt, ...)
{
    if (*used >= cap - 1U) return;
    va_list ap;
    va_start(ap, fmt);
    int n = stm_vsnprintf(buf + *used, cap - *used, fmt, ap);
    va_end(ap);
    if (n > 0) *used += (size_t)n < cap - *used ? (size_t)n : cap - *used - 1U;
}

static int format_with_prefix(char *buf, size_t cap, stm_log_level_t level,
                              const char *tag, const char *file, int line,
                              const char *fmt, va_list ap)
{
    size_t used = 0;
    buf[0] = '\0';
#if STM_LOG_USE_COLORS
    append_format(buf, cap, &used, "%s", s_lvl_color[level]);
#endif
    append_format(buf, cap, &used, "%c (%lu) %s", s_lvl_chr[level],
                  (unsigned long)log_tick(), tag ? tag : "");
#if STM_LOG_INCLUDE_FILE_LINE
    if (file) {
        const char *base = file;
        for (const char *p = file; *p; ++p)
            if (*p == '/' || *p == '\\') base = p + 1;
        append_format(buf, cap, &used, " [%s:%d]", base, line);
    }
#else
    (void)file; (void)line;
#endif
    append_format(buf, cap, &used, ": ");
    if (used < cap - 1U) {
        int n = stm_vsnprintf(buf + used, cap - used, fmt, ap);
        if (n < 0) return -1;
        used += (size_t)n < cap - used ? (size_t)n : cap - used - 1U;
    }
#if STM_LOG_USE_COLORS
    // 即使正文被截断，也要保留颜色复位，避免污染后续终端输出。
    if (used > cap - 5U) used = cap - 5U;
    memcpy(buf + used, "\033[0m", 4U);
    used += 4U;
    buf[used] = '\0';
#endif
    return (int)used;
}

/* ─── 公共 API 实现 ─── */

/**
 * @brief 切换全局默认级别
 */
void stm_log_set_level(stm_log_level_t level) {
    LOCK();
    s_level = level;
    UNLOCK();
}

/**
 * @brief 一步完成 init + set_output：装 callback + 设 level + flush 早期 buffer
 *
 * @note  STM_LOG_USE_MUTEX=1 时本函数末尾创建 FreeRTOS recursive mutex，
 *        须在 scheduler 启动后调用。输出为 NULL 时不冲掉已缓存的日志。
 */
void stm_log_init_output(stm_log_output_fn output, stm_log_level_t level) {
#if STM_LOG_USE_MUTEX
    if (!s_mutex) {
        s_mutex = xSemaphoreCreateRecursiveMutex();
    }
#endif
    LOCK();
    s_output = output;
    s_level  = level;
    UNLOCK();
    early_flush();
}

/**
 * @brief 运行时切换输出 callback（NULL = 暂停输出；绑定非空输出时 flush 早期 buffer）
 */
void stm_log_set_output(stm_log_output_fn output) {
    LOCK();
    s_output = output;
    UNLOCK();
    early_flush();
}

/**
 * @brief 注册 / 更新某 tag 的级别（NONE = 静音该 tag）
 *
 * @note  NONE 现在持久化到 per-tag 表，占一条名额；如需"删除回退全局"用 unset
 */
void stm_log_set_tag_level(const char *tag, stm_log_level_t level) {
    if (!tag) {
        return;
    }

    LOCK();
    for (uint8_t i = 0; i < s_tag_count; i++) {
        if (strcmp(s_tags[i].tag, tag) == 0) {
            s_tags[i].level = level;
            UNLOCK();
            return;
        }
    }

    if (s_tag_count < STM_LOG_MAX_TAGS) {
        s_tags[s_tag_count].tag   = tag;
        s_tags[s_tag_count].level = level;
        s_tag_count++;
    }
    UNLOCK();
}

/**
 * @brief 删除某 tag 的 per-tag 配置（让该 tag 回退到全局默认）
 */
void stm_log_unset_tag_level(const char *tag) {
    if (!tag) {
        return;
    }

    LOCK();
    for (uint8_t i = 0; i < s_tag_count; i++) {
        if (strcmp(s_tags[i].tag, tag) == 0) {
            for (uint8_t j = i; j + 1u < s_tag_count; j++) {
                s_tags[j] = s_tags[j + 1u];
            }
            s_tag_count--;
            UNLOCK();
            return;
        }
    }
    UNLOCK();
}

/**
 * @brief 查询某 tag 的生效级别（读路径，不加锁）
 */
stm_log_level_t stm_log_get_tag_level(const char *tag) {
    return resolve_level(tag);
}

/**
 * @brief 通用日志输出入口 — s_output 就绪则走 callback，否则进 ring buffer
 *
 * @note  热路径不加锁；默认单调用者，不承诺多任务线程安全。
 */
void stm_log(stm_log_level_t level, const char *tag, const char *fmt, ...) {
    if (!STM_LOG_ENABLED || !fmt || level < STM_LOG_LVL_ERROR
        || level > STM_LOG_LVL_VERBOSE || level > resolve_level(tag)) {
        return;
    }

    char    buf[STM_LOG_BUFFER_SIZE];
    va_list ap;
    va_start(ap, fmt);
    int n = format_with_prefix(buf, sizeof(buf), level, tag, NULL, 0, fmt, ap);
    va_end(ap);

    if (n <= 0) {
        return;
    }

    if (s_output) {
        emit(buf, (uint16_t)n);
    } else {
        early_write(buf, (uint16_t)n);
    }
}

#if STM_LOG_INCLUDE_FILE_LINE
/**
 * @brief 带 file:line 的日志输出（STM_LOG_INCLUDE_FILE_LINE=1 时启用）
 */
void stm_log_fl(stm_log_level_t level, const char *file, int line,
                const char *tag, const char *fmt, ...) {
    if (!STM_LOG_ENABLED || !fmt || level < STM_LOG_LVL_ERROR
        || level > STM_LOG_LVL_VERBOSE || level > resolve_level(tag)) {
        return;
    }

    char    buf[STM_LOG_BUFFER_SIZE];
    va_list ap;
    va_start(ap, fmt);
    int n = format_with_prefix(buf, sizeof(buf), level, tag, file, line, fmt, ap);
    va_end(ap);

    if (n <= 0) {
        return;
    }

    if (s_output) {
        emit(buf, (uint16_t)n);
    } else {
        early_write(buf, (uint16_t)n);
    }
}
#endif

/**
 * @brief hex buffer 打印 — 每行 bytes_per_line 字节
 */
void stm_log_hex(stm_log_level_t level, const char *tag,
                 const void *buf, uint16_t len, uint16_t bytes_per_line) {
    if (!STM_LOG_ENABLED || !buf || !len || !bytes_per_line
        || level < STM_LOG_LVL_ERROR || level > STM_LOG_LVL_VERBOSE
        || level > resolve_level(tag)) return;
    const uint8_t *p = buf;
    // 使用 32 位偏移，避免 len=65535 时 uint16_t 回绕。
    for (uint32_t i = 0; i < len;) {
        char out[STM_LOG_BUFFER_SIZE];
        size_t used = 0;
        out[0] = '\0';
        uint32_t count = (uint32_t)len - i;
        if (count > bytes_per_line) count = bytes_per_line;
#if STM_LOG_USE_COLORS
        append_format(out, sizeof out, &used, "%s", s_lvl_color[level]);
#endif
        append_format(out, sizeof out, &used, "%c (%lu) %s: 0x[ ",
                      s_lvl_chr[level], (unsigned long)log_tick(), tag ? tag : "");
        for (uint32_t j = 0; j < count && used < sizeof out - 1U; ++j)
            append_format(out, sizeof out, &used, "%02x ", p[i + j]);
        append_format(out, sizeof out, &used, "]");
#if STM_LOG_USE_COLORS
        if (used > sizeof out - 5U) used = sizeof out - 5U;
        memcpy(out + used, "\033[0m", 4U);
        used += 4U;
#endif
        if (s_output) emit(out, (uint16_t)used);
        else early_write(out, (uint16_t)used);
        i += count;
    }
}
