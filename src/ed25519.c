/* ed25519.c — Ed25519 (RFC 8032) over GF(2^255-19) fe25519 arithmetic.
 *
 * Structure:
 *   - scalar arithmetic mod L = 2^252 + 27742317777372353535851937790883648493
 *   - extended twisted Edwards coordinates (X:Y:Z:T), complete addition
 *   - constant-time Montgomery-style ladder for scalar multiplication
 *   - point (de)compression with sqrt via x^((p+3)/8) and sqrt(-1)
 */
#include "ed25519.h"
#include "fe25519.h"
#include "sha512.h"
#include "rand.h"
#include <string.h>

/* ── Scalar arithmetic mod L ───────────────────────────────────── */

/* L in 64-bit little-endian words */
static const uint64_t SC_L[4] = {
    0x5812631a5cf5d3edULL,
    0x14def9dea2f79cd6ULL,
    0x0000000000000000ULL,
    0x1000000000000000ULL
};

/* a >= b ? */
static int sc_ge(const uint64_t a[4], const uint64_t b[4])
{
    for (int i = 3; i >= 0; i--) {
        if (a[i] > b[i]) return 1;
        if (a[i] < b[i]) return 0;
    }
    return 1; /* equal */
}

/* a -= b (a >= b assumed) */
static void sc_sub(uint64_t a[4], const uint64_t b[4])
{
    uint64_t borrow = 0;
    for (int i = 0; i < 4; i++) {
        uint64_t bi = b[i] + borrow;
        borrow = (bi < b[i]) || (a[i] < bi) ? 1 : 0;
        a[i] -= bi;
    }
}

/* Reduce an arbitrary little-endian integer (inlen <= 64 bytes) mod L.
 * Bit-serial: r = r*2 + bit; if r >= L: r -= L. Constant-time. */
static void sc_reduce_wide(uint8_t out[32], const uint8_t *in, size_t inlen)
{
    uint64_t r[4] = {0};

    for (int bit = (int)inlen * 8 - 1; bit >= 0; bit--) {
        uint64_t carry = (in[bit / 8] >> (bit & 7)) & 1;
        for (int i = 0; i < 4; i++) {
            uint64_t nxt = r[i] >> 63;
            r[i] = (r[i] << 1) | carry;
            carry = nxt;
        }
        if (sc_ge(r, SC_L)) sc_sub(r, SC_L);
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 8; j++) {
            out[i * 8 + j] = (uint8_t)(r[i] >> (j * 8));
        }
    }
}

static void sc_reduce(uint8_t out[32], const uint8_t in[64])
{
    sc_reduce_wide(out, in, 64);
}

/* out = a * b + c (mod L); all 32-byte little-endian scalars */
static void sc_muladd(uint8_t out[32], const uint8_t a[32],
                      const uint8_t b[32], const uint8_t c[32])
{
    uint8_t prod[64] = {0};

    /* schoolbook a*b into prod (32x32 bytes, fits in 64 bytes since a,b < 2^253) */
    for (int i = 0; i < 32; i++) {
        uint32_t carry = 0;
        for (int j = 0; j < 32; j++) {
            uint32_t v = (uint32_t)prod[i + j] + (uint32_t)a[i] * (uint32_t)b[j] + carry;
            prod[i + j] = (uint8_t)(v & 0xFF);
            carry = v >> 8;
        }
        int k = i + 32;
        while (carry != 0 && k < 64) {
            uint32_t v = (uint32_t)prod[k] + carry;
            prod[k] = (uint8_t)(v & 0xFF);
            carry = v >> 8;
            k++;
        }
    }

    /* add c */
    uint32_t carry = 0;
    for (int i = 0; i < 32; i++) {
        uint32_t v = (uint32_t)prod[i] + (uint32_t)c[i] + carry;
        prod[i] = (uint8_t)(v & 0xFF);
        carry = v >> 8;
    }
    for (int i = 32; i < 64 && carry != 0; i++) {
        uint32_t v = (uint32_t)prod[i] + carry;
        prod[i] = (uint8_t)(v & 0xFF);
        carry = v >> 8;
    }

    sc_reduce_wide(out, prod, 64);
}

/* s < L ? (by little-endian value) */
static int sc_lt_L(const uint8_t s[32])
{
    for (int i = 31; i >= 0; i--) {
        uint8_t lb = (uint8_t)(SC_L[i / 8] >> ((i % 8) * 8));
        if (s[i] < lb) return 1;
        if (s[i] > lb) return 0;
    }
    return 0; /* equal is not < */
}

/* ── Group: extended twisted Edwards coordinates ───────────────── */

typedef struct {
    fe25519 X, Y, Z, T;
} ge_p3;

typedef struct {
    fe25519 d, d2;    /* d = -121665/121666, d2 = 2d */
    fe25519 sqrtm1;   /* sqrt(-1) = 2^((p-1)/4) */
} ed_ctx;

static void fe_from_u64(fe25519 out, uint64_t v)
{
    fe25519_set_zero(out);
    out[0] = v;
}

static void fe_neg(fe25519 out, const fe25519 a)
{
    fe25519 zero;
    fe25519_set_zero(zero);
    fe25519_sub(out, zero, a);
}

static int fe_equal(const fe25519 a, const fe25519 b)
{
    uint8_t ba[32], bb[32];
    fe25519_tobytes(ba, a);
    fe25519_tobytes(bb, b);
    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= ba[i] ^ bb[i];
    return diff == 0;
}

static void ge_identity(ge_p3 *p)
{
    fe25519_set_zero(p->X);
    fe25519_set_one(p->Y);
    fe25519_set_one(p->Z);
    fe25519_set_zero(p->T);
}

/* r = p + q  (complete addition for a=-1 twisted Edwards) */
static void ge_add(ge_p3 *r, const ge_p3 *p, const ge_p3 *q, const ed_ctx *c)
{
    fe25519 a, b, cc, d, e, f, g, h, t, u;

    fe25519_sub(t, p->Y, p->X);
    fe25519_sub(u, q->Y, q->X);
    fe25519_mul(a, t, u);                     /* A = (Y1-X1)(Y2-X2) */

    fe25519_add(t, p->Y, p->X);
    fe25519_add(u, q->Y, q->X);
    fe25519_mul(b, t, u);                     /* B = (Y1+X1)(Y2+X2) */

    fe25519_mul(t, p->T, q->T);
    fe25519_mul(cc, t, c->d2);                /* C = T1 * 2d * T2 */

    fe25519_mul(t, p->Z, q->Z);
    fe25519_add(d, t, t);                     /* D = 2*Z1*Z2 */

    fe25519_sub(e, b, a);                     /* E = B-A */
    fe25519_sub(f, d, cc);                    /* F = D-C */
    fe25519_add(g, d, cc);                    /* G = D+C */
    fe25519_add(h, b, a);                     /* H = B+A */

    fe25519_mul(r->X, e, f);
    fe25519_mul(r->Y, g, h);
    fe25519_mul(r->T, e, h);
    fe25519_mul(r->Z, f, g);
}

static void ge_negate(ge_p3 *r, const ge_p3 *p)
{
    fe_neg(r->X, p->X);
    fe25519_copy(r->Y, p->Y);
    fe25519_copy(r->Z, p->Z);
    fe_neg(r->T, p->T);
}

static void ge_cswap(ge_p3 *a, ge_p3 *b, int swap)
{
    fe25519_cswap(a->X, b->X, swap);
    fe25519_cswap(a->Y, b->Y, swap);
    fe25519_cswap(a->Z, b->Z, swap);
    fe25519_cswap(a->T, b->T, swap);
}

/* r = scalar * p (255-bit scalar, constant-time ladder) */
static void ge_scalarmult(ge_p3 *r, const ge_p3 *p, const uint8_t scalar[32],
                          const ed_ctx *c)
{
    ge_p3 r0, r1, t;
    ge_identity(&r0);
    r1 = *p;

    for (int bit = 254; bit >= 0; bit--) {
        int b = (scalar[bit / 8] >> (bit & 7)) & 1;
        ge_cswap(&r0, &r1, b);
        ge_add(&t, &r0, &r1, c);
        ge_add(&r0, &r0, &r0, c);
        r1 = t;
        ge_cswap(&r0, &r1, b);
    }
    *r = r0;
}

/* Compress a point to 32 bytes: y with x's parity in the top bit. */
static void ge_tobytes(uint8_t out[32], const ge_p3 *p)
{
    fe25519 zi, x, y;
    fe25519_invert(zi, p->Z);
    fe25519_mul(x, p->X, zi);
    fe25519_mul(y, p->Y, zi);
    fe25519_tobytes(out, y);
    out[31] |= (uint8_t)(fe25519_parity(x) << 7);
}

/* Decompress 32 bytes into a point. Returns 0 on success. */
static int ge_frombytes(ge_p3 *p, const uint8_t in[32], const ed_ctx *c)
{
    uint8_t yb[32];
    memcpy(yb, in, 32);
    int sign = (yb[31] >> 7) & 1;
    yb[31] &= 0x7F;

    fe25519 y, y2, u, v, inv_v, x2, cand, cand2, x;
    fe25519_frombytes(y, yb);

    fe25519_sq(y2, y);
    fe_from_u64(u, 1);
    fe25519_sub(u, y2, u);            /* u = y^2 - 1 */
    fe25519_mul(v, y2, c->d);
    fe_from_u64(x2, 1);
    fe25519_add(v, v, x2);            /* v = d*y^2 + 1 */

    fe25519_invert(inv_v, v);
    fe25519_mul(x2, u, inv_v);        /* x2 = u/v */

    fe25519_pow(cand, x2, FE25519_P58_EXP);  /* x2^((p+3)/8) */
    fe25519_sq(cand2, cand);

    if (fe_equal(cand2, x2)) {
        fe25519_copy(x, cand);
    } else {
        fe_neg(u, x2);
        if (fe_equal(cand2, u)) {
            fe25519_mul(x, cand, c->sqrtm1);
        } else {
            return -1; /* not a square: invalid point */
        }
    }

    if (fe25519_iszero(x) && sign) return -1;
    if (fe25519_parity(x) != sign) fe_neg(x, x);

    fe25519_copy(p->X, x);
    fe25519_copy(p->Y, y);
    fe25519_set_one(p->Z);
    fe25519_mul(p->T, x, y);
    return 0;
}

/* Ed25519 base point, compressed (y = 4/5, x even). */
static const uint8_t ED25519_B_COMPRESSED[32] = {
    0x58, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66
};

static void ed_init(ed_ctx *c)
{
    fe25519 num, den, inv_den, two;

    /* d = -121665 / 121666 */
    fe_from_u64(num, 121665);
    fe_neg(num, num);
    fe_from_u64(den, 121666);
    fe25519_invert(inv_den, den);
    fe25519_mul(c->d, num, inv_den);
    fe25519_add(c->d2, c->d, c->d);

    /* sqrt(-1) = 2^((p-1)/4) */
    fe_from_u64(two, 2);
    fe25519_pow(c->sqrtm1, two, FE25519_P14_EXP);
}

/* ── Key derivation / signing ──────────────────────────────────── */

static void expand_seed(uint8_t az[64], uint8_t a[32], const uint8_t seed[32])
{
    sha512(seed, 32, az);
    memcpy(a, az, 32);
    a[0]  &= 248;
    a[31] &= 127;
    a[31] |= 64;
}

int ed25519_pubkey_from_seed(uint8_t pub[ED25519_PUBLIC_KEY_SIZE],
                             const uint8_t seed[ED25519_SEED_SIZE])
{
    ed_ctx c;
    ge_p3 B, A;
    uint8_t az[64], a[32];

    ed_init(&c);
    if (ge_frombytes(&B, ED25519_B_COMPRESSED, &c) != 0) return -1;

    expand_seed(az, a, seed);
    ge_scalarmult(&A, &B, a, &c);
    ge_tobytes(pub, &A);
    return 0;
}

void ed25519_keypair(uint8_t pub[ED25519_PUBLIC_KEY_SIZE],
                     uint8_t seed[ED25519_SEED_SIZE])
{
    if (rand_bytes(seed, ED25519_SEED_SIZE) != 0) {
        memset(seed, 0, ED25519_SEED_SIZE);
        memset(pub, 0, ED25519_PUBLIC_KEY_SIZE);
        return;
    }
    ed25519_pubkey_from_seed(pub, seed);
}

int ed25519_sign(uint8_t sig[ED25519_SIGNATURE_SIZE],
                 const uint8_t seed[ED25519_SEED_SIZE],
                 const uint8_t *msg, size_t msg_len)
{
    ed_ctx c;
    ge_p3 B, R;
    uint8_t az[64], a[32];
    uint8_t r64[64], r[32], k64[64], k[32], s[32];
    sha512_ctx_t hctx;

    ed_init(&c);
    if (ge_frombytes(&B, ED25519_B_COMPRESSED, &c) != 0) return -1;

    expand_seed(az, a, seed);

    /* r = SHA512(prefix || M) mod L */
    sha512_init(&hctx);
    sha512_update(&hctx, az + 32, 32);
    sha512_update(&hctx, msg, msg_len);
    sha512_final(&hctx, r64);
    sc_reduce(r, r64);

    /* R = r*B */
    ge_scalarmult(&R, &B, r, &c);
    ge_tobytes(sig, &R);

    /* A = a*B, k = SHA512(R || A || M) mod L */
    {
        ge_p3 A;
        ge_scalarmult(&A, &B, a, &c);
        ge_tobytes(sig + 32, &A); /* temporarily store A at sig+32 */

        sha512_init(&hctx);
        sha512_update(&hctx, sig, 32);
        sha512_update(&hctx, sig + 32, 32);
        sha512_update(&hctx, msg, msg_len);
        sha512_final(&hctx, k64);
        sc_reduce(k, k64);
    }

    /* S = r + k*a mod L; sig = R || S */
    sc_muladd(s, k, a, r);
    memcpy(sig + 32, s, 32);
    return 0;
}

int ed25519_verify(const uint8_t pub[ED25519_PUBLIC_KEY_SIZE],
                   const uint8_t sig[ED25519_SIGNATURE_SIZE],
                   const uint8_t *msg, size_t msg_len)
{
    ed_ctx c;
    ge_p3 B, A, SB, kA, negkA, Q;
    uint8_t k64[64], k[32], Qb[32];
    sha512_ctx_t hctx;

    if (!sc_lt_L(sig + 32)) return -1; /* S must be < L */

    ed_init(&c);
    if (ge_frombytes(&B, ED25519_B_COMPRESSED, &c) != 0) return -1;
    if (ge_frombytes(&A, pub, &c) != 0) return -1;

    /* k = SHA512(R || A || M) mod L */
    sha512_init(&hctx);
    sha512_update(&hctx, sig, 32);
    sha512_update(&hctx, pub, 32);
    sha512_update(&hctx, msg, msg_len);
    sha512_final(&hctx, k64);
    sc_reduce(k, k64);

    /* Check [S]B - [k]A == R */
    ge_scalarmult(&SB, &B, sig + 32, &c);
    ge_scalarmult(&kA, &A, k, &c);
    ge_negate(&negkA, &kA);
    ge_add(&Q, &SB, &negkA, &c);
    ge_tobytes(Qb, &Q);

    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= Qb[i] ^ sig[i];
    return diff == 0 ? 0 : -1;
}
