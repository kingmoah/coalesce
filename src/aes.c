/* aes.c — AES (FIPS 197) block cipher + CTR mode, no external deps. */
#include "aes.h"
#include <string.h>

/* ── S-box (FIPS 197 Figure 7) ─────────────────────────────────── */

static const uint8_t SBOX[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static const uint8_t RCON[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

#define ROTWORD(w) (((w) << 8) | ((w) >> 24))
#define SUBWORD(w) ((uint32_t)SBOX[((w) >> 24) & 0xFF] << 24 | \
                    (uint32_t)SBOX[((w) >> 16) & 0xFF] << 16 | \
                    (uint32_t)SBOX[((w) >> 8)  & 0xFF] << 8  | \
                    (uint32_t)SBOX[(w) & 0xFF])

/* ── Key expansion (FIPS 197 §5.2) ─────────────────────────────── */

int aes_set_encrypt_key(aes_ctx_t *ctx, const uint8_t *key, int key_bits)
{
    if (!ctx || !key) return -1;

    int nk;      /* key length in 32-bit words */
    int rounds;
    switch (key_bits) {
        case 128: nk = 4;  rounds = 10; break;
        case 192: nk = 6;  rounds = 12; break;
        case 256: nk = 8;  rounds = 14; break;
        default: return -1;
    }
    ctx->rounds = rounds;

    int total = 4 * (rounds + 1);
    for (int i = 0; i < nk; i++) {
        ctx->rk[i] = ((uint32_t)key[4 * i] << 24) |
                     ((uint32_t)key[4 * i + 1] << 16) |
                     ((uint32_t)key[4 * i + 2] << 8) |
                     ((uint32_t)key[4 * i + 3]);
    }

    for (int i = nk; i < total; i++) {
        uint32_t temp = ctx->rk[i - 1];
        if (i % nk == 0) {
            temp = SUBWORD(ROTWORD(temp)) ^ ((uint32_t)RCON[i / nk] << 24);
        } else if (nk > 6 && i % nk == 4) {
            temp = SUBWORD(temp);
        }
        ctx->rk[i] = ctx->rk[i - nk] ^ temp;
    }
    return 0;
}

/* ── Block encryption (FIPS 197 §5.1) ──────────────────────────── */

static void add_round_key(uint8_t s[16], const uint32_t rk[4])
{
    for (int c = 0; c < 4; c++) {
        s[4 * c]     ^= (uint8_t)(rk[c] >> 24);
        s[4 * c + 1] ^= (uint8_t)(rk[c] >> 16);
        s[4 * c + 2] ^= (uint8_t)(rk[c] >> 8);
        s[4 * c + 3] ^= (uint8_t)(rk[c]);
    }
}

static void sub_bytes(uint8_t s[16])
{
    for (int i = 0; i < 16; i++) s[i] = SBOX[s[i]];
}

static void shift_rows(uint8_t s[16])
{
    uint8_t t;
    /* row 1: left by 1 */
    t = s[1]; s[1] = s[5]; s[5] = s[9]; s[9] = s[13]; s[13] = t;
    /* row 2: left by 2 */
    t = s[2]; s[2] = s[10]; s[10] = t;
    t = s[6]; s[6] = s[14]; s[14] = t;
    /* row 3: left by 3 (= right by 1) */
    t = s[15]; s[15] = s[11]; s[11] = s[7]; s[7] = s[3]; s[3] = t;
}

static uint8_t xtime(uint8_t x)
{
    return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1b : 0x00));
}

static void mix_columns(uint8_t s[16])
{
    for (int c = 0; c < 4; c++) {
        uint8_t *p = s + 4 * c;
        uint8_t a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3];
        uint8_t all = a0 ^ a1 ^ a2 ^ a3;
        p[0] ^= all ^ xtime(a0 ^ a1);
        p[1] ^= all ^ xtime(a1 ^ a2);
        p[2] ^= all ^ xtime(a2 ^ a3);
        p[3] ^= all ^ xtime(a3 ^ a0);
    }
}

void aes_encrypt_block(const aes_ctx_t *ctx,
                       const uint8_t in[AES_BLOCK_SIZE],
                       uint8_t out[AES_BLOCK_SIZE])
{
    uint8_t s[16];
    memcpy(s, in, 16);

    add_round_key(s, &ctx->rk[0]);
    for (int r = 1; r < ctx->rounds; r++) {
        sub_bytes(s);
        shift_rows(s);
        mix_columns(s);
        add_round_key(s, &ctx->rk[4 * r]);
    }
    /* final round: no MixColumns */
    sub_bytes(s);
    shift_rows(s);
    add_round_key(s, &ctx->rk[4 * ctx->rounds]);

    memcpy(out, s, 16);
}

/* ── CTR mode (NIST SP 800-38A) ────────────────────────────────── */

static void ctr_increment(uint8_t ctr[16])
{
    /* 128-bit big-endian increment */
    for (int i = 15; i >= 0; i--) {
        if (++ctr[i] != 0) break;
    }
}

int aes_ctr_init(aes_ctr_ctx_t *ctx, const uint8_t *key, int key_bits,
                 const uint8_t iv[AES_BLOCK_SIZE])
{
    if (!ctx || !key || !iv) return -1;

    memset(ctx, 0, sizeof(*ctx));
    if (aes_set_encrypt_key(&ctx->aes, key, key_bits) != 0) return -1;
    memcpy(ctx->nonce_counter, iv, AES_BLOCK_SIZE);
    return 0;
}

void aes_ctr_crypt(aes_ctr_ctx_t *ctx, const uint8_t *in, uint8_t *out, size_t len)
{
    size_t i = 0;
    while (i < len) {
        if (ctx->stream_offset == 0) {
            aes_encrypt_block(&ctx->aes, ctx->nonce_counter, ctx->stream);
            ctr_increment(ctx->nonce_counter);
        }
        size_t take = AES_BLOCK_SIZE - ctx->stream_offset;
        if (take > len - i) take = len - i;
        for (size_t k = 0; k < take; k++) {
            out[i + k] = (uint8_t)(in[i + k] ^ ctx->stream[ctx->stream_offset + k]);
        }
        ctx->stream_offset = (ctx->stream_offset + take) % AES_BLOCK_SIZE;
        i += take;
    }
}
