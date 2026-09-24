#ifndef COALESCE_ED25519_H
#define COALESCE_ED25519_H

#include <stdint.h>
#include <stddef.h>

/* Ed25519 (RFC 8032) — pure C, built on fe25519 + sha512. */

#define ED25519_SEED_SIZE         32
#define ED25519_PUBLIC_KEY_SIZE   32
#define ED25519_SIGNATURE_SIZE    64

/* Generate a fresh random keypair: seed = private key, pub = public key. */
void ed25519_keypair(uint8_t pub[ED25519_PUBLIC_KEY_SIZE],
                     uint8_t seed[ED25519_SEED_SIZE]);

/* Derive the public key from a 32-byte seed. Returns 0 on success. */
int ed25519_pubkey_from_seed(uint8_t pub[ED25519_PUBLIC_KEY_SIZE],
                             const uint8_t seed[ED25519_SEED_SIZE]);

/* Sign msg with the seed. Returns 0 on success. */
int ed25519_sign(uint8_t sig[ED25519_SIGNATURE_SIZE],
                 const uint8_t seed[ED25519_SEED_SIZE],
                 const uint8_t *msg, size_t msg_len);

/* Verify sig over msg with pub. Returns 0 if valid, -1 otherwise. */
int ed25519_verify(const uint8_t pub[ED25519_PUBLIC_KEY_SIZE],
                   const uint8_t sig[ED25519_SIGNATURE_SIZE],
                   const uint8_t *msg, size_t msg_len);

#endif /* COALESCE_ED25519_H */
