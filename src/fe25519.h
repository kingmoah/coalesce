#ifndef COALESCE_FE25519_H
#define COALESCE_FE25519_H

#include <stdint.h>

/* Field arithmetic modulo p = 2^255 - 19 (51-bit limbs, u128 products).
 * Shared by curve25519.c (X25519) and ed25519.c (Edwards curve). */

typedef uint64_t fe25519[5];

void fe25519_frombytes(fe25519 out, const uint8_t in[32]);
void fe25519_tobytes(uint8_t out[32], const fe25519 a);

void fe25519_add(fe25519 out, const fe25519 a, const fe25519 b);      /* out = a + b (weak) */
void fe25519_sub(fe25519 out, const fe25519 a, const fe25519 b);      /* out = a - b + 2p  */
void fe25519_mul(fe25519 out, const fe25519 a, const fe25519 b);
void fe25519_sq(fe25519 out, const fe25519 a);
void fe25519_mul121666(fe25519 out, const fe25519 a);                 /* out = a * 121666  */
void fe25519_copy(fe25519 out, const fe25519 a);
void fe25519_set_zero(fe25519 out);
void fe25519_set_one(fe25519 out);
void fe25519_cswap(fe25519 a, fe25519 b, int swap);                   /* conditional swap */

void fe25519_pow(fe25519 out, const fe25519 a, const uint8_t e[32]);  /* out = a^e, e little-endian */
void fe25519_invert(fe25519 out, const fe25519 a);                    /* out = a^(p-2) */

int  fe25519_iszero(const fe25519 a);   /* 1 if a == 0 mod p */
int  fe25519_parity(const fe25519 a);   /* 1 if canonical value is odd */

extern const uint8_t FE25519_P_MINUS_2[32]; /* p-2, LE (for invert) */
extern const uint8_t FE25519_P58_EXP[32];   /* (p+3)/8, LE (for sqrt)  */
extern const uint8_t FE25519_P14_EXP[32];   /* (p-1)/4, LE (for sqrt(-1)) */

#endif /* COALESCE_FE25519_H */
