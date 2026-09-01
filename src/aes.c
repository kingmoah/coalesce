#include "aes.h"
#include <string.h>

/* ── Raw AES key setup ────────────────────────────────────────── */

int aes_set_encrypt_key(aes_ctx_t *ctx, const uint8_t *key, int key_bits)
{
    if (!ctx || !key) return -1;
    if (key_bits != 128 && key_bits != 192 && key_bits != 256) return -1;

    mbedtls_aes_init(&ctx->ctx);
    ctx->key_bits = key_bits;

    if (mbedtls_aes_setkey_enc(&ctx->ctx, key, (unsigned int)key_bits) != 0)
        return -1;

    return 0;
}

void aes_encrypt_block(const aes_ctx_t *ctx,
                       const uint8_t in[AES_BLOCK_SIZE],
                       uint8_t out[AES_BLOCK_SIZE])
{
    /* mbedtls_aes_crypt_ecb takes a non-const context due to internal state;
     * cast away const — the encrypt key schedule is already stored and not mutated. */
    mbedtls_aes_crypt_ecb((mbedtls_aes_context *)&ctx->ctx,
                          MBEDTLS_AES_ENCRYPT, in, out);
}

/* ── AES-CTR ──────────────────────────────────────────────────── */

int aes_ctr_init(aes_ctr_ctx_t *ctx, const uint8_t *key, int key_bits,
                 const uint8_t iv[AES_BLOCK_SIZE])
{
    if (!ctx || !key || !iv) return -1;
    if (key_bits != 128 && key_bits != 192 && key_bits != 256) return -1;

    mbedtls_aes_init(&ctx->aes);

    if (mbedtls_aes_setkey_enc(&ctx->aes, key, (unsigned int)key_bits) != 0)
        return -1;

    memcpy(ctx->nonce_counter, iv, AES_BLOCK_SIZE);
    memset(ctx->stream, 0, AES_BLOCK_SIZE);
    ctx->stream_offset = 0;

    return 0;
}

void aes_ctr_crypt(aes_ctr_ctx_t *ctx, const uint8_t *in, uint8_t *out, size_t len)
{
    /* mbedtls_aes_crypt_ctr manages the counter and keystream buffer internally. */
    mbedtls_aes_crypt_ctr(&ctx->aes, len,
                          &ctx->stream_offset,
                          ctx->nonce_counter,
                          ctx->stream,
                          in, out);
}
