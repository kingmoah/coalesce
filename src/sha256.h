#ifndef COALESCE_SHA256_H
#define COALESCE_SHA256_H

#include <stdint.h>
#include <stddef.h>

#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE  64

/* ── SHA-256 (FIPS 180-4) ─────────────────────────────────────── */

typedef struct {
    uint32_t state[8];
    uint64_t bits;        /* total message length in bits */
    uint8_t  buf[SHA256_BLOCK_SIZE];
    size_t   buflen;
} sha256_ctx_t;

void sha256_init(sha256_ctx_t *ctx);
void sha256_update(sha256_ctx_t *ctx, const void *data, size_t len);
void sha256_final(sha256_ctx_t *ctx, uint8_t digest[SHA256_DIGEST_SIZE]);
void sha256(const void *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]);

/* ── HMAC-SHA256 (RFC 2104 / FIPS 198-1) ──────────────────────── */

typedef struct {
    sha256_ctx_t inner;
    sha256_ctx_t outer;
} hmac_sha256_ctx_t;

void hmac_sha256_init(hmac_sha256_ctx_t *ctx, const void *key, size_t key_len);
void hmac_sha256_update(hmac_sha256_ctx_t *ctx, const void *data, size_t len);
void hmac_sha256_final(hmac_sha256_ctx_t *ctx, uint8_t digest[SHA256_DIGEST_SIZE]);
void hmac_sha256(const void *key, size_t key_len,
                 const void *data, size_t data_len,
                 uint8_t digest[SHA256_DIGEST_SIZE]);

#endif /* COALESCE_SHA256_H */
