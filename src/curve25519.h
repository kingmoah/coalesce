#ifndef COALESCE_CURVE25519_H
#define COALESCE_CURVE25519_H

#include <stdint.h>
#include <stddef.h>

#define CURVE25519_KEY_SIZE 32

/* Computes out = scalar * point  (X25519 ECDH) */
int curve25519_eval(uint8_t out[CURVE25519_KEY_SIZE],
                    const uint8_t scalar[CURVE25519_KEY_SIZE],
                    const uint8_t point[CURVE25519_KEY_SIZE]);

/* Computes public key = scalar * base_point (9) */
int curve25519_base(uint8_t out[CURVE25519_KEY_SIZE],
                    const uint8_t scalar[CURVE25519_KEY_SIZE]);

/* Generates a cryptographically random 32-byte private key (clamped) */
void curve25519_generate_private(uint8_t private_key[CURVE25519_KEY_SIZE]);

#endif /* COALESCE_CURVE25519_H */
