#include "sha256.h"
#include <string.h>

/* ── SHA-256 ──────────────────────────────────────────────────── */

void sha256_init(sha256_ctx_t *ctx)
{
    mbedtls_sha256_init(&ctx->ctx);
    mbedtls_sha256_starts(&ctx->ctx, 0 /* is224=0 → SHA-256 */);
}

void sha256_update(sha256_ctx_t *ctx, const void *data, size_t len)
{
    mbedtls_sha256_update(&ctx->ctx, (const unsigned char *)data, len);
}

void sha256_final(sha256_ctx_t *ctx, uint8_t digest[SHA256_DIGEST_SIZE])
{
    mbedtls_sha256_finish(&ctx->ctx, digest);
    mbedtls_sha256_free(&ctx->ctx);
}

void sha256(const void *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE])
{
    mbedtls_sha256((const unsigned char *)data, len, digest, 0 /* SHA-256 */);
}

/* ── HMAC-SHA256 ──────────────────────────────────────────────── */

void hmac_sha256_init(hmac_sha256_ctx_t *ctx, const void *key, size_t key_len)
{
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_init(&ctx->ctx);
    mbedtls_md_setup(&ctx->ctx, info, 1 /* hmac=1 */);
    mbedtls_md_hmac_starts(&ctx->ctx, (const unsigned char *)key, key_len);
}

void hmac_sha256_update(hmac_sha256_ctx_t *ctx, const void *data, size_t len)
{
    mbedtls_md_hmac_update(&ctx->ctx, (const unsigned char *)data, len);
}

void hmac_sha256_final(hmac_sha256_ctx_t *ctx, uint8_t digest[SHA256_DIGEST_SIZE])
{
    mbedtls_md_hmac_finish(&ctx->ctx, digest);
    mbedtls_md_free(&ctx->ctx);
}

void hmac_sha256(const void *key, size_t key_len,
                 const void *data, size_t data_len,
                 uint8_t digest[SHA256_DIGEST_SIZE])
{
    hmac_sha256_ctx_t ctx;
    hmac_sha256_init(&ctx, key, key_len);
    hmac_sha256_update(&ctx, data, data_len);
    hmac_sha256_final(&ctx, digest);
}
