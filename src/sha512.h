#ifndef COALESCE_SHA512_H
#define COALESCE_SHA512_H

#include <stdint.h>
#include <stddef.h>

#define SHA512_DIGEST_SIZE 64
#define SHA512_BLOCK_SIZE  128

/* ── SHA-512 (FIPS 180-4) — used by Ed25519 (RFC 8032) ────────── */

typedef struct {
    uint64_t state[8];
    uint64_t bits_hi;      /* total message length in bits (high word) */
    uint64_t bits_lo;      /* (low word) */
    uint8_t  buf[SHA512_BLOCK_SIZE];
    size_t   buflen;
} sha512_ctx_t;

void sha512_init(sha512_ctx_t *ctx);
void sha512_update(sha512_ctx_t *ctx, const void *data, size_t len);
void sha512_final(sha512_ctx_t *ctx, uint8_t digest[SHA512_DIGEST_SIZE]);
void sha512(const void *data, size_t len, uint8_t digest[SHA512_DIGEST_SIZE]);

#endif /* COALESCE_SHA512_H */
