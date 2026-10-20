/* Own implementation of the published SHA-256 algorithm; MIT, see LICENSE. */
#include "pure_sha256.h"

static const uint32_t round_constants[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

static uint32_t rotate(uint32_t n, unsigned bits) { return (n >> bits) | (n << (32 - bits)); }
static uint32_t load_be(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static void transform(pure_sha256_context *c, const uint8_t block[64])
{
    uint32_t words[64];
    for (unsigned i = 0; i < 16; ++i) words[i] = load_be(block + 4 * i);
    for (unsigned i = 16; i < 64; ++i) {
        uint32_t a = words[i - 15], b = words[i - 2];
        uint32_t s0 = rotate(a, 7) ^ rotate(a, 18) ^ (a >> 3);
        uint32_t s1 = rotate(b, 17) ^ rotate(b, 19) ^ (b >> 10);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }
    uint32_t a = c->state[0], b = c->state[1], d = c->state[3], e = c->state[4];
    uint32_t f = c->state[5], g = c->state[6], h = c->state[7], x = c->state[2];
    for (unsigned i = 0; i < 64; ++i) {
        uint32_t s1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
        uint32_t choice = (e & f) ^ (~e & g);
        uint32_t first = h + s1 + choice + round_constants[i] + words[i];
        uint32_t s0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
        uint32_t majority = (a & b) ^ (a & x) ^ (b & x);
        uint32_t second = s0 + majority;
        h = g; g = f; f = e; e = d + first; d = x; x = b; b = a; a = first + second;
    }
    c->state[0] += a; c->state[1] += b; c->state[2] += x; c->state[3] += d;
    c->state[4] += e; c->state[5] += f; c->state[6] += g; c->state[7] += h;
}

void pure_sha256_init(pure_sha256_context *c)
{
    c->state[0] = 0x6a09e667u; c->state[1] = 0xbb67ae85u;
    c->state[2] = 0x3c6ef372u; c->state[3] = 0xa54ff53au;
    c->state[4] = 0x510e527fu; c->state[5] = 0x9b05688cu;
    c->state[6] = 0x1f83d9abu; c->state[7] = 0x5be0cd19u;
    c->total_bytes = 0; c->buffered = 0;
}

void pure_sha256_update(pure_sha256_context *c, const void *input, size_t bytes)
{
    const uint8_t *data = input;
    c->total_bytes += bytes;
    while (bytes) {
        if (!c->buffered && bytes >= 64) {
            transform(c, data); data += 64; bytes -= 64;
        } else {
            size_t take = 64 - c->buffered;
            if (take > bytes) take = bytes;
            for (size_t i = 0; i < take; ++i) c->block[c->buffered + i] = data[i];
            c->buffered += take; data += take; bytes -= take;
            if (c->buffered == 64) { transform(c, c->block); c->buffered = 0; }
        }
    }
}

void pure_sha256_final(pure_sha256_context *c, uint8_t digest[32])
{
    uint64_t bits = c->total_bytes << 3;
    c->block[c->buffered++] = 0x80;
    if (c->buffered > 56) {
        while (c->buffered < 64) c->block[c->buffered++] = 0;
        transform(c, c->block); c->buffered = 0;
    }
    while (c->buffered < 56) c->block[c->buffered++] = 0;
    for (unsigned i = 0; i < 8; ++i) c->block[63 - i] = (uint8_t)(bits >> (8 * i));
    transform(c, c->block);
    for (unsigned i = 0; i < 32; ++i) digest[i] = (uint8_t)(c->state[i / 4] >> (24 - (i % 4) * 8));
    c->buffered = 0;
}

void pure_sha256_hex(const uint8_t digest[32], char out[65])
{
    static const char alphabet[] = "0123456789abcdef";
    for (unsigned i = 0; i < 32; ++i) {
        out[2 * i] = alphabet[digest[i] >> 4]; out[2 * i + 1] = alphabet[digest[i] & 15];
    }
    out[64] = 0;
}

void pure_sha256_data(const void *data, size_t bytes, char out[65])
{
    pure_sha256_context c;
    uint8_t digest[32];
    pure_sha256_init(&c); pure_sha256_update(&c, data, bytes);
    pure_sha256_final(&c, digest); pure_sha256_hex(digest, out);
}
