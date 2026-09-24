#include "curve25519.h"
#include "fe25519.h"
#include "rand.h"
#include <string.h>

/* ── curve25519_eval: out = scalar * point (RFC 7748 X25519) ──── */

int curve25519_eval(uint8_t out[CURVE25519_KEY_SIZE],
                    const uint8_t scalar[CURVE25519_KEY_SIZE],
                    const uint8_t point[CURVE25519_KEY_SIZE])
{
    uint8_t e[32];
    memcpy(e, scalar, 32);
    /* RFC 7748 clamping */
    e[0]  &= 248;
    e[31] &= 127;
    e[31] |= 64;

    /* u-coordinate (top bit masked per RFC 7748) */
    fe25519 x1, x2, z2, x3, z3;
    fe25519_frombytes(x1, point);

    fe25519_set_one(x2);
    fe25519_set_zero(z2);
    fe25519_copy(x3, x1);
    fe25519_set_one(z3);

    int swap = 0;
    for (int t = 254; t >= 0; t--) {
        int k_t = (e[t / 8] >> (t & 7)) & 1;
        swap ^= k_t;
        fe25519_cswap(x2, x3, swap);
        fe25519_cswap(z2, z3, swap);
        swap = k_t;

        fe25519 A, AA, B, BB, E, C, D, DA, CB;
        fe25519_add(A, x2, z2);
        fe25519_sq(AA, A);
        fe25519_sub(B, x2, z2);
        fe25519_sq(BB, B);
        fe25519_sub(E, AA, BB);
        fe25519_add(C, x3, z3);
        fe25519_sub(D, x3, z3);
        fe25519_mul(DA, D, A);
        fe25519_mul(CB, C, B);

        fe25519 t1, t2;
        fe25519_add(t1, DA, CB);
        fe25519_sq(x3, t1);
        fe25519_sub(t2, DA, CB);
        fe25519_sq(t2, t2);
        fe25519_mul(z3, x1, t2);

        fe25519_mul(x2, AA, BB);
        /* z2 = E * (BB + a24 * E), a24 = 121666 — equal to RFC 7748's
         * E * (AA + 121665 * E) but keeps all terms positive in limbs */
        fe25519_mul121666(t1, E);
        fe25519_add(t1, BB, t1);
        fe25519_mul(z2, E, t1);
    }
    fe25519_cswap(x2, x3, swap);
    fe25519_cswap(z2, z3, swap);

    fe25519 zi, res;
    fe25519_invert(zi, z2);
    fe25519_mul(res, x2, zi);
    fe25519_tobytes(out, res);
    return 0;
}

/* ── curve25519_base: out = scalar * G (u = 9) ────────────────── */

int curve25519_base(uint8_t out[CURVE25519_KEY_SIZE],
                    const uint8_t scalar[CURVE25519_KEY_SIZE])
{
    static const uint8_t base_point[CURVE25519_KEY_SIZE] = {9};
    return curve25519_eval(out, scalar, base_point);
}

/* ── curve25519_generate_private ─────────────────────────────── */

void curve25519_generate_private(uint8_t private_key[CURVE25519_KEY_SIZE])
{
    if (rand_bytes(private_key, CURVE25519_KEY_SIZE) != 0) {
        memset(private_key, 0, CURVE25519_KEY_SIZE);
        return;
    }
    /* RFC 7748 clamping */
    private_key[0]  &= 248;
    private_key[31] &= 127;
    private_key[31] |= 64;
}
