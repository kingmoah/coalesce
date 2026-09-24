/* fe25519.c — GF(2^255-19) arithmetic in 51-bit limbs (u128 products).
 * Structure follows the well-known ref/donna-style 51-bit implementations. */
#include "fe25519.h"
#include <string.h>

typedef unsigned __int128 u128;

#define MASK51 0x7FFFFFFFFFFFFULL

/* 2p in 51-bit limbs: 2p = 2^256 - 38 = (2^52 - 38) + sum_{i=1..4} (2^52 - 2) * 2^(51i).
 * Used by subtraction to keep limbs positive (inputs must have limbs < 2^52). */
static const uint64_t P2[5] = {
    (1ULL << 52) - 38,
    (1ULL << 52) - 2,
    (1ULL << 52) - 2,
    (1ULL << 52) - 2,
    (1ULL << 52) - 2
};

/* Exponents (little-endian bytes):
 * p-2    = 0x7fff...ffeb
 * (p+3)/8 = 2^252 - 2
 * (p-1)/4 = 2^253 - 5 */
const uint8_t FE25519_P_MINUS_2[32] = {
    0xeb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f
};
const uint8_t FE25519_P58_EXP[32] = {
    0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f
};
const uint8_t FE25519_P14_EXP[32] = {
    0xfb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1f
};

/* ── Serialization ─────────────────────────────────────────────── */

void fe25519_frombytes(fe25519 out, const uint8_t in[32])
{
    uint64_t x[4];
    for (int i = 0; i < 4; i++) {
        x[i] = ((uint64_t)in[8 * i + 0]) |
               ((uint64_t)in[8 * i + 1] << 8) |
               ((uint64_t)in[8 * i + 2] << 16) |
               ((uint64_t)in[8 * i + 3] << 24) |
               ((uint64_t)in[8 * i + 4] << 32) |
               ((uint64_t)in[8 * i + 5] << 40) |
               ((uint64_t)in[8 * i + 6] << 48) |
               ((uint64_t)in[8 * i + 7] << 56);
    }
    out[0] = x[0] & MASK51;
    out[1] = ((x[0] >> 51) | (x[1] << 13)) & MASK51;
    out[2] = ((x[1] >> 38) | (x[2] << 26)) & MASK51;
    out[3] = ((x[2] >> 25) | (x[3] << 39)) & MASK51;
    out[4] = (x[3] >> 12) & MASK51;
}

void fe25519_tobytes(uint8_t out[32], const fe25519 a)
{
    fe25519 in;
    memcpy(in, a, sizeof(fe25519));

    /* two carry passes */
    for (int pass = 0; pass < 2; pass++) {
        uint64_t c = in[0] >> 51; in[0] &= MASK51; in[1] += c;
        c = in[1] >> 51; in[1] &= MASK51; in[2] += c;
        c = in[2] >> 51; in[2] &= MASK51; in[3] += c;
        c = in[3] >> 51; in[3] &= MASK51; in[4] += c;
        c = in[4] >> 51; in[4] &= MASK51; in[0] += c * 19;
    }

    /* conditional subtract of p (via +19 trick) */
    fe25519 t;
    t[0] = in[0] + 19;
    uint64_t c = t[0] >> 51; t[0] &= MASK51;
    t[1] = in[1] + c; c = t[1] >> 51; t[1] &= MASK51;
    t[2] = in[2] + c; c = t[2] >> 51; t[2] &= MASK51;
    t[3] = in[3] + c; c = t[3] >> 51; t[3] &= MASK51;
    t[4] = in[4] + c; c = t[4] >> 51; t[4] &= MASK51;

    /* if the value was >= p the carry out of limb 4 is 1; use t, else in */
    uint64_t mask = (uint64_t)0 - c; /* all ones if c != 0 */
    for (int i = 0; i < 5; i++) {
        in[i] = (in[i] & ~mask) | (t[i] & mask);
    }

    uint64_t x[4];
    x[0] = in[0] | (in[1] << 51);
    x[1] = (in[1] >> 13) | (in[2] << 38);
    x[2] = (in[2] >> 26) | (in[3] << 25);
    x[3] = (in[3] >> 39) | (in[4] << 12);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 8; j++) {
            out[i * 8 + j] = (uint8_t)((x[i] >> (j * 8)) & 0xFF);
        }
    }
}

/* ── Arithmetic ────────────────────────────────────────────────── */

void fe25519_add(fe25519 out, const fe25519 a, const fe25519 b)
{
    for (int i = 0; i < 5; i++) out[i] = a[i] + b[i];
}

void fe25519_sub(fe25519 out, const fe25519 a, const fe25519 b)
{
    /* out = a + 2p - b, keeps limbs positive (input limbs must be < 2p) */
    for (int i = 0; i < 5; i++) out[i] = (a[i] + P2[i]) - b[i];
}

void fe25519_mul(fe25519 out, const fe25519 a, const fe25519 b)
{
    u128 t[9] = {0};
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            t[i + j] += (u128)a[i] * (u128)b[j];
        }
    }
    for (int i = 0; i < 4; i++) {
        t[i] += t[i + 5] * 19;
    }

    u128 c = 0;
    for (int i = 0; i < 5; i++) {
        u128 v = t[i] + c;
        out[i] = (uint64_t)(v & MASK51);
        c = v >> 51;
    }
    out[0] += (uint64_t)(c * 19);

    c = out[0] >> 51; out[0] &= MASK51; out[1] += (uint64_t)c;
    c = out[1] >> 51; out[1] &= MASK51; out[2] += (uint64_t)c;
    c = out[2] >> 51; out[2] &= MASK51; out[3] += (uint64_t)c;
    c = out[3] >> 51; out[3] &= MASK51; out[4] += (uint64_t)c;
    c = out[4] >> 51; out[4] &= MASK51; out[0] += (uint64_t)(c * 19);
}

void fe25519_sq(fe25519 out, const fe25519 a)
{
    fe25519_mul(out, a, a);
}

void fe25519_mul121666(fe25519 out, const fe25519 a)
{
    u128 c = 0;
    for (int i = 0; i < 5; i++) {
        u128 v = (u128)a[i] * 121666 + c;
        out[i] = (uint64_t)(v & MASK51);
        c = v >> 51;
    }
    out[0] += (uint64_t)(c * 19);

    c = out[0] >> 51; out[0] &= MASK51; out[1] += (uint64_t)c;
    c = out[1] >> 51; out[1] &= MASK51; out[2] += (uint64_t)c;
    c = out[2] >> 51; out[2] &= MASK51; out[3] += (uint64_t)c;
    c = out[3] >> 51; out[3] &= MASK51; out[4] += (uint64_t)c;
    c = out[4] >> 51; out[4] &= MASK51; out[0] += (uint64_t)(c * 19);
}

void fe25519_copy(fe25519 out, const fe25519 a)
{
    memcpy(out, a, sizeof(fe25519));
}

void fe25519_set_zero(fe25519 out)
{
    memset(out, 0, sizeof(fe25519));
}

void fe25519_set_one(fe25519 out)
{
    memset(out, 0, sizeof(fe25519));
    out[0] = 1;
}

void fe25519_cswap(fe25519 a, fe25519 b, int swap)
{
    uint64_t mask = (uint64_t)0 - (uint64_t)(swap != 0);
    for (int i = 0; i < 5; i++) {
        uint64_t x = mask & (a[i] ^ b[i]);
        a[i] ^= x;
        b[i] ^= x;
    }
}

/* ── Exponentiation ────────────────────────────────────────────── */

void fe25519_pow(fe25519 out, const fe25519 a, const uint8_t e[32])
{
    fe25519 r;
    fe25519_set_one(r);

    for (int bit = 255; bit >= 0; bit--) {
        fe25519_sq(r, r);
        if ((e[bit / 8] >> (bit & 7)) & 1) {
            fe25519_mul(r, r, a);
        }
    }
    fe25519_copy(out, r);
}

void fe25519_invert(fe25519 out, const fe25519 a)
{
    fe25519_pow(out, a, FE25519_P_MINUS_2);
}

int fe25519_iszero(const fe25519 a)
{
    uint8_t b[32];
    fe25519_tobytes(b, a);
    uint8_t acc = 0;
    for (int i = 0; i < 32; i++) acc |= b[i];
    return acc == 0;
}

int fe25519_parity(const fe25519 a)
{
    uint8_t b[32];
    fe25519_tobytes(b, a);
    return b[0] & 1;
}
