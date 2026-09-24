#ifndef COALESCE_AES_H
#define COALESCE_AES_H

#include <stdint.h>
#include <stddef.h>

#define AES_BLOCK_SIZE 16
#define AES_MAX_ROUNDS 14 /* 10 (128-bit) / 12 (192-bit) / 14 (256-bit) */

/* ── Raw AES context (encrypt-only) ────────────────────────────── */

typedef struct {
    uint32_t rk[4 * (AES_MAX_ROUNDS + 1)];
    int      rounds;
} aes_ctx_t;

/* ── AES-CTR streaming context ─────────────────────────────────── */

typedef struct {
    aes_ctx_t             aes;
    uint8_t               nonce_counter[AES_BLOCK_SIZE]; /* current counter block */
    uint8_t               stream[AES_BLOCK_SIZE];        /* keystream buffer      */
    size_t                stream_offset;                 /* bytes consumed        */
} aes_ctr_ctx_t;

/* Standard AES key setup and block encryption */
int  aes_set_encrypt_key(aes_ctx_t *ctx, const uint8_t *key, int key_bits);
void aes_encrypt_block(const aes_ctx_t *ctx,
                       const uint8_t in[AES_BLOCK_SIZE],
                       uint8_t out[AES_BLOCK_SIZE]);

/* AES-CTR mode (encrypt == decrypt) */
int  aes_ctr_init(aes_ctr_ctx_t *ctx, const uint8_t *key, int key_bits,
                  const uint8_t iv[AES_BLOCK_SIZE]);
void aes_ctr_crypt(aes_ctr_ctx_t *ctx, const uint8_t *in, uint8_t *out, size_t len);

#endif /* COALESCE_AES_H */
