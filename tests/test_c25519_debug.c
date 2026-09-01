#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "../src/curve25519.h"

typedef uint64_t fe[5];
typedef unsigned __int128 u128;
#define MASK51 0x7FFFFFFFFFFFFULL

/* Multiples of p = 2^255 - 19 in 51-bit limbs */
static const uint64_t P2[5] = {
    0xFFFFFFFFFFFDAULL, /* 2 * (2^51 - 19) */
    0xFFFFFFFFFFFEULL, /* 2 * (2^51 - 1) */
    0xFFFFFFFFFFFEULL,
    0xFFFFFFFFFFFEULL,
    0xFFFFFFFFFFFEULL
};

static void fexpand(fe out, const uint8_t in[32])
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

static void fcontract(uint8_t out[32], fe in)
{
    for (int pass = 0; pass < 2; pass++) {
        uint64_t c = in[0] >> 51; in[0] &= MASK51; in[1] += c;
        c = in[1] >> 51; in[1] &= MASK51; in[2] += c;
        c = in[2] >> 51; in[2] &= MASK51; in[3] += c;
        c = in[3] >> 51; in[3] &= MASK51; in[4] += c;
        c = in[4] >> 51; in[4] &= MASK51; in[0] += c * 19;
    }

    fe t;
    t[0] = in[0] + 19;
    uint64_t c = t[0] >> 51; t[0] &= MASK51;
    t[1] = in[1] + c;
    c = t[1] >> 51; t[1] &= MASK51;
    t[2] = in[2] + c;
    c = t[2] >> 51; t[2] &= MASK51;
    t[3] = in[3] + c;
    c = t[3] >> 51; t[3] &= MASK51;
    t[4] = in[4] + c;
    c = t[4] >> 51; t[4] &= MASK51;

    if (c != 0) {
        memcpy(in, t, sizeof(fe));
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

static void fmul(fe out, const fe a, const fe b)
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

    c = out[0] >> 51; out[0] &= MASK51;
    out[1] += (uint64_t)c;
    c = out[1] >> 51; out[1] &= MASK51;
    out[2] += (uint64_t)c;
    c = out[2] >> 51; out[2] &= MASK51;
    out[3] += (uint64_t)c;
    c = out[3] >> 51; out[3] &= MASK51;
    out[4] += (uint64_t)c;
    c = out[4] >> 51; out[4] &= MASK51;
    out[0] += (uint64_t)(c * 19);
}

static void fsquare(fe out, const fe a)
{
    fmul(out, a, a);
}

static void fscalar_product(fe out, const fe in, uint64_t s)
{
    u128 t[5];
    for (int i = 0; i < 5; i++) {
        t[i] = (u128)in[i] * s;
    }
    u128 c = 0;
    for (int i = 0; i < 5; i++) {
        u128 v = t[i] + c;
        out[i] = (uint64_t)(v & MASK51);
        c = v >> 51;
    }
    out[0] += (uint64_t)(c * 19);

    c = out[0] >> 51; out[0] &= MASK51;
    out[1] += (uint64_t)c;
    c = out[1] >> 51; out[1] &= MASK51;
    out[2] += (uint64_t)c;
    c = out[2] >> 51; out[2] &= MASK51;
    out[3] += (uint64_t)c;
    c = out[3] >> 51; out[3] &= MASK51;
    out[4] += (uint64_t)c;
    c = out[4] >> 51; out[4] &= MASK51;
    out[0] += (uint64_t)(c * 19);
}

static void fsum(fe out, const fe in)
{
    for (int i = 0; i < 5; i++) {
        out[i] += in[i];
    }
}

static void fdifference(fe out, const fe in1, const fe in2)
{
    for (int i = 0; i < 5; i++) {
        out[i] = (in1[i] + P2[i]) - in2[i];
    }
}

static void fscalar_product(fe out, const fe in, uint64_t s)
{
    u128 c = 0;
    for (int i = 0; i < 5; i++) {
        u128 v = (u128)in[i] * s + c;
        out[i] = (uint64_t)(v & MASK51);
        c = v >> 51;
    }
    out[0] += (uint64_t)(c * 19);

    c = out[0] >> 51;
    out[0] &= MASK51;
    out[1] += (uint64_t)c;
}

static void fswap(fe a, fe b, int condition)
{
    uint64_t mask = -(uint64_t)condition;
    for (int i = 0; i < 5; i++) {
        uint64_t x = mask & (a[i] ^ b[i]);
        a[i] ^= x;
        b[i] ^= x;
    }
}

int curve25519_eval(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32])
{
    uint8_t e[32];
    memcpy(e, scalar, 32);
    e[0] &= 248;
    e[31] &= 127;
    e[31] |= 64;

    fe x1, x2, z2, x3, z3, a, b, c, d, da, cb, aa, bb;
    fexpand(x1, point);

    memset(x2, 0, sizeof(fe)); x2[0] = 1;
    memset(z2, 0, sizeof(fe));
    memcpy(x3, x1, sizeof(fe));
    memset(z3, 0, sizeof(fe)); z3[0] = 1;

    int swap = 0;
    for (int pos = 254; pos >= 0; pos--) {
        int b_bit = (e[pos / 8] >> (pos & 7)) & 1;
        swap ^= b_bit;
        fswap(x2, x3, swap);
        fswap(z2, z3, swap);
        swap = b_bit;

        memcpy(a, x2, sizeof(fe)); fsum(a, z2);
        fdifference(b, x2, z2);
        memcpy(c, x3, sizeof(fe)); fsum(c, z3);
        fdifference(d, x3, z3);

        fsquare(aa, a);
        fsquare(bb, b);
        fmul(da, d, a);
        fmul(cb, c, b);

        memcpy(x3, da, sizeof(fe)); fsum(x3, cb); fsquare(x3, x3);
        fdifference(z3, da, cb); fsquare(z3, z3); fmul(z3, z3, x1);

        fmul(x2, aa, bb);
        fdifference(z2, aa, bb);
        fscalar_product(a, z2, 121665);
        fsum(a, bb);
        fmul(z2, z2, a);
    }
    fswap(x2, x3, swap);
    fswap(z2, z3, swap);

    finvert(z2, z2);
    fmul(x2, x2, z2);
    fcontract(out, x2);
    return 0;
}

int main(void)
{
    const uint8_t alice_priv[32] = {
        0x77, 0x07, 0x6d, 0x0a, 0x73, 0x18, 0xa5, 0x7d,
        0x3c, 0x16, 0xc1, 0x72, 0x51, 0xb2, 0x66, 0x45,
        0xdf, 0x4c, 0x2f, 0x87, 0xeb, 0xc0, 0x99, 0x2a,
        0xab, 0x17, 0x7f, 0xba, 0x51, 0xdb, 0x09, 0x2a
    };
    const uint8_t expected_alice_pub[32] = {
        0x85, 0x20, 0xf0, 0x09, 0x89, 0x30, 0xa7, 0x54,
        0x74, 0x8b, 0x7d, 0xdc, 0xb4, 0x3e, 0xf7, 0x5a,
        0x0d, 0xbf, 0x3a, 0x0d, 0x26, 0x38, 0x15, 0xe4,
        0x2b, 0x60, 0x46, 0x11, 0x81, 0x9b, 0x32, 0x4f
    };
    uint8_t base_point[32] = {9};
    uint8_t alice_pub[32];
    curve25519_eval(alice_pub, alice_priv, base_point);

    printf("alice_pub: ");
    for (int i = 0; i < 32; i++) printf("%02x", alice_pub[i]);
    printf("\nexpected:  ");
    for (int i = 0; i < 32; i++) printf("%02x", expected_alice_pub[i]);
    printf("\n");

    if (memcmp(alice_pub, expected_alice_pub, 32) == 0) {
        printf("PASS: curve25519_eval test vector passed!\n");
    } else {
        printf("FAIL: curve25519_eval mismatch!\n");
    }
    return 0;
}
