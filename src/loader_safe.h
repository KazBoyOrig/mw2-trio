#pragma once
#include <stddef.h>
#include <stdint.h>

/* Own bounded byte/format helpers. Volatile loops avoid CRT memory libcalls on
   the attach path; compile its portable code with -ffreestanding -fno-builtin. */
static inline void loader_zero(void *out, size_t bytes)
{
    volatile uint8_t *p = out;
    for (size_t i = 0; i < bytes; ++i) p[i] = 0;
}
static inline void loader_copy(void *out, const void *in, size_t bytes)
{
    volatile uint8_t *d = out;
    const volatile uint8_t *s = in;
    for (size_t i = 0; i < bytes; ++i) d[i] = s[i];
}
static inline int loader_equal(const void *left, const void *right, size_t bytes)
{
    const volatile uint8_t *a = left, *b = right;
    for (size_t i = 0; i < bytes; ++i) if (a[i] != b[i]) return 0;
    return 1;
}
typedef struct loader_text { char *data; size_t capacity, used; } loader_text;
static inline void loader_char(loader_text *out, char c)
{
    if (out->used + 1 < out->capacity) { out->data[out->used++] = c; out->data[out->used] = 0; }
}
static inline void loader_string(loader_text *out, const char *text)
{
    for (size_t i = 0; text[i] && out->used + 1 < out->capacity; ++i) loader_char(out, text[i]);
}
static inline void loader_hex(loader_text *out, uint32_t value, unsigned digits)
{
    static const char hex[] = "0123456789abcdef";
    while (digits) { --digits; loader_char(out, hex[(value >> (digits * 4)) & 15]); }
}
static inline void loader_unsigned(loader_text *out, uint32_t value)
{
    char digits[10]; unsigned n = 0;
    do { digits[n++] = (char)('0' + value % 10); value /= 10; } while (value);
    while (n) loader_char(out, digits[--n]);
}
static inline void loader_signed(loader_text *out, int32_t value)
{
    if (value < 0) { loader_char(out, '-'); loader_unsigned(out, 0u - (uint32_t)value); }
    else loader_unsigned(out, (uint32_t)value);
}
