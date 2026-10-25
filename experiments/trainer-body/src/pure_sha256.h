#pragma once
#include <stddef.h>
#include <stdint.h>

/* Own portable SHA-256. No OS, CRT, provider, lazy initialization or allocation. */
typedef struct pure_sha256_context {
    uint32_t state[8];
    uint64_t total_bytes;
    size_t buffered;
    uint8_t block[64];
} pure_sha256_context;

void pure_sha256_init(pure_sha256_context *);
void pure_sha256_update(pure_sha256_context *, const void *, size_t);
void pure_sha256_final(pure_sha256_context *, uint8_t digest[32]);
void pure_sha256_hex(const uint8_t digest[32], char out[65]);
void pure_sha256_data(const void *, size_t, char out[65]);
