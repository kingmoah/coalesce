/* sha512.c — SHA-512, no external dependencies. */
#include "sha512.h"
#include <string.h>

static const uint64_t K512[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))

static void sha512_block(uint64_t state[8], const uint8_t block[128])
{
    uint64_t w[80];
    for (int i = 0; i < 16; i++) {
        const uint8_t *p = block + i * 8;
        w[i] = ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) |
               ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
               ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
               ((uint64_t)p[6] << 8)  |  (uint64_t)p[7];
    }
    for (int i = 16; i < 80; i++) {
        uint64_t s0 = ROTR64(w[i - 15], 1) ^ ROTR64(w[i - 15], 8) ^ (w[i - 15] >> 7);
        uint64_t s1 = ROTR64(w[i - 2], 19) ^ ROTR64(w[i - 2], 61) ^ (w[i - 2] >> 6);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint64_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint64_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 80; i++) {
        uint64_t S1 = ROTR64(e, 14) ^ ROTR64(e, 18) ^ ROTR64(e, 41);
        uint64_t ch = (e & f) ^ (~e & g);
        uint64_t t1 = h + S1 + ch + K512[i] + w[i];
        uint64_t S0 = ROTR64(a, 28) ^ ROTR64(a, 34) ^ ROTR64(a, 39);
        uint64_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint64_t t2 = S0 + mj;

        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void sha512_init(sha512_ctx_t *ctx)
{
    ctx->state[0] = 0x6a09e667f3bcc908ULL; ctx->state[1] = 0xbb67ae8584caa73bULL;
    ctx->state[2] = 0x3c6ef372fe94f82bULL; ctx->state[3] = 0xa54ff53a5f1d36f1ULL;
    ctx->state[4] = 0x510e527fade682d1ULL; ctx->state[5] = 0x9b05688c2b3e6c1fULL;
    ctx->state[6] = 0x1f83d9abfb41bd6bULL; ctx->state[7] = 0x5be0cd19137e2179ULL;
    ctx->bits_hi = 0;
    ctx->bits_lo = 0;
    ctx->buflen = 0;
}

void sha512_update(sha512_ctx_t *ctx, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;

    /* bits += len*8 (128-bit accumulate) */
    ctx->bits_lo += (uint64_t)len << 3;
    ctx->bits_hi += (uint64_t)len >> 61;
    if (ctx->bits_lo < ((uint64_t)len << 3)) ctx->bits_hi++; /* won't trigger; kept for clarity */

    if (ctx->buflen > 0) {
        size_t need = SHA512_BLOCK_SIZE - ctx->buflen;
        size_t take = len < need ? len : need;
        memcpy(ctx->buf + ctx->buflen, p, take);
        ctx->buflen += take;
        p += take;
        len -= take;
        if (ctx->buflen == SHA512_BLOCK_SIZE) {
            sha512_block(ctx->state, ctx->buf);
            ctx->buflen = 0;
        }
    }
    while (len >= SHA512_BLOCK_SIZE) {
        sha512_block(ctx->state, p);
        p += SHA512_BLOCK_SIZE;
        len -= SHA512_BLOCK_SIZE;
    }
    if (len > 0) {
        memcpy(ctx->buf, p, len);
        ctx->buflen = len;
    }
}

void sha512_final(sha512_ctx_t *ctx, uint8_t digest[SHA512_DIGEST_SIZE])
{
    uint64_t hi = ctx->bits_hi;
    uint64_t lo = ctx->bits_lo;

    ctx->buf[ctx->buflen++] = 0x80;
    if (ctx->buflen > 112) {
        memset(ctx->buf + ctx->buflen, 0, SHA512_BLOCK_SIZE - ctx->buflen);
        sha512_block(ctx->state, ctx->buf);
        ctx->buflen = 0;
    }
    memset(ctx->buf + ctx->buflen, 0, 112 - ctx->buflen);

    /* length in bits, big-endian 128-bit */
    for (int i = 0; i < 8; i++)
        ctx->buf[112 + i] = (uint8_t)(hi >> (56 - i * 8));
    for (int i = 0; i < 8; i++)
        ctx->buf[120 + i] = (uint8_t)(lo >> (56 - i * 8));
    sha512_block(ctx->state, ctx->buf);

    for (int i = 0; i < 8; i++) {
        digest[i * 8]     = (uint8_t)(ctx->state[i] >> 56);
        digest[i * 8 + 1] = (uint8_t)(ctx->state[i] >> 48);
        digest[i * 8 + 2] = (uint8_t)(ctx->state[i] >> 40);
        digest[i * 8 + 3] = (uint8_t)(ctx->state[i] >> 32);
        digest[i * 8 + 4] = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 8 + 5] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 8 + 6] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 8 + 7] = (uint8_t)(ctx->state[i]);
    }
}

void sha512(const void *data, size_t len, uint8_t digest[SHA512_DIGEST_SIZE])
{
    sha512_ctx_t ctx;
    sha512_init(&ctx);
    sha512_update(&ctx, data, len);
    sha512_final(&ctx, digest);
}
