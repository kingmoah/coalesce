#ifndef COALESCE_SHA256_H
#define COALESCE_SHA256_H

#include <stdint.h>
#include <stddef.h>

/* Pull in the mbedtls SHA-256 and MD (HMAC) context types */
#include "mbedtls/sha256.h"
#include "mbedtls/md.h"

#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE  64

/* ── SHA-256 ──────────────────────────────────────────────────── */

typedef struct {
    mbedtls_sha256_context ctx;
} sha256_ctx_t;

void sha256_init(sha256_ctx_t *ctx);
void sha256_update(sha256_ctx_t *ctx, const void *data, size_t len);
void sha256_final(sha256_ctx_t *ctx, uint8_t digest[SHA256_DIGEST_SIZE]);
void sha256(const void *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]);

/* ── HMAC-SHA256 ──────────────────────────────────────────────── */

typedef struct {
    mbedtls_md_context_t ctx;
} hmac_sha256_ctx_t;

void hmac_sha256_init(hmac_sha256_ctx_t *ctx, const void *key, size_t key_len);
void hmac_sha256_update(hmac_sha256_ctx_t *ctx, const void *data, size_t len);
void hmac_sha256_final(hmac_sha256_ctx_t *ctx, uint8_t digest[SHA256_DIGEST_SIZE]);
void hmac_sha256(const void *key, size_t key_len,
                 const void *data, size_t data_len,
                 uint8_t digest[SHA256_DIGEST_SIZE]);

#endif /* COALESCE_SHA256_H */
