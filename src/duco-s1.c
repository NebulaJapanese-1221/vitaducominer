#include "duco-s1.h"
#include <string.h>
#include <stdio.h>

static const uint32_t sha1_k[4] = {
    0x5a827999u,
    0x6ed9eba1u,
    0x8f1bbcdcu,
    0xca62c1d6u
};

static uint32_t rotl32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

static void sha1_transform(SHA1_CTX *ctx, const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               (uint32_t)block[i * 4 + 3];
    }
    for (int i = 16; i < 80; i++) {
        w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];

    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = sha1_k[0];
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = sha1_k[1];
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = sha1_k[2];
        } else {
            f = b ^ c ^ d;
            k = sha1_k[3];
        }
        uint32_t temp = rotl32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotl32(b, 30);
        b = a;
        a = temp;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

void sha1_init(SHA1_CTX *ctx) {
    ctx->state[0] = 0x67452301u;
    ctx->state[1] = 0xefcdab89u;
    ctx->state[2] = 0x98badcfeu;
    ctx->state[3] = 0x10325476u;
    ctx->state[4] = 0xc3d2e1f0u;
    ctx->count[0] = 0;
    ctx->count[1] = 0;
}

void sha1_update(SHA1_CTX *ctx, const uint8_t *data, size_t len) {
    size_t index = (size_t)((ctx->count[0] >> 3) & 0x3f);
    ctx->count[0] += (uint32_t)(len * 8);
    if (ctx->count[0] < (uint32_t)(len * 8)) {
        ctx->count[1]++;
    }
    ctx->count[1] += (uint32_t)(((uint64_t)len * 8) >> 32);
    size_t part = 64 - index;
    size_t i = 0;
    if (len >= part) {
        memcpy(&ctx->buffer[index], data, part);
        sha1_transform(ctx, ctx->buffer);
        i = part;
        while (i + 64 <= len) {
            sha1_transform(ctx, &data[i]);
            i += 64;
        }
        index = 0;
    }
    memcpy(&ctx->buffer[index], &data[i], len - i);
}

void sha1_final(SHA1_CTX *ctx, uint8_t digest[20]) {
    uint64_t bits = ((uint64_t)ctx->count[1] << 32) | ctx->count[0];
    uint8_t pad[64];
    size_t index = (size_t)((ctx->count[0] >> 3) & 0x3f);
    size_t padlen = (index < 56) ? (56 - index) : (120 - index);
    pad[0] = 0x80;
    memset(pad + 1, 0, padlen - 1);
    for (size_t i = 0; i < padlen; i += 64) {
        sha1_update(ctx, pad, 64);
    }
    uint8_t bits_le[8];
    for (int i = 0; i < 8; i++) {
        bits_le[i] = (uint8_t)(bits >> (8 * i));
    }
    sha1_update(ctx, bits_le, 8);
    for (int i = 0; i < 20; i++) {
        digest[i] = (uint8_t)(ctx->state[i >> 2] >> (24 - 8 * (i & 3)));
    }
}

void duco_s1_hash(const char *data, uint32_t difficulty, uint32_t nonce, uint8_t digest[20]) {
    char input[256];
    int n = snprintf(input, sizeof(input), "%s:%u:%u", data, difficulty, nonce);
    if (n < 0 || (size_t)n >= sizeof(input)) {
        n = (int)sizeof(input) - 1;
    }
    SHA1_CTX ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, (const uint8_t *)input, (size_t)n);
    sha1_final(&ctx, digest);
}

int duco_s1_mine(const char *data, uint32_t difficulty, uint32_t *nonce_out, uint8_t digest[20], uint32_t max_iter) {
    uint32_t nonce = 0;
    for (uint32_t i = 0; i < max_iter; i++) {
        duco_s1_hash(data, difficulty, nonce, digest);
        int leading_zeros = 0;
        for (int b = 0; b < 20; b++) {
            uint8_t byte = digest[b];
            for (int bit = 7; bit >= 0; bit--) {
                if ((byte >> bit) & 1u) {
                    break;
                }
                leading_zeros++;
            }
            if (leading_zeros > 0 && (digest[b] != 0)) {
                break;
            }
        }
        if ((uint32_t)leading_zeros >= difficulty) {
            *nonce_out = nonce;
            return 1;
        }
        nonce++;
    }
    return 0;
}
