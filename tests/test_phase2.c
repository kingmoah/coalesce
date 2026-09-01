#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/sha256.h"
#include "../src/curve25519.h"
#include "../src/aes.h"
#include "../src/kex.h"

static void test_sha256(void)
{
    /* Test vector: SHA256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad */
    const uint8_t expected_abc[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    uint8_t digest[32];
    sha256("abc", 3, digest);
    assert(memcmp(digest, expected_abc, 32) == 0);

    /* HMAC-SHA256 test vector */
    const char *key = "key";
    const char *data = "The quick brown fox jumps over the lazy dog";
    /* Expected: f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8 */
    const uint8_t expected_hmac[32] = {
        0xf7, 0xbc, 0x83, 0xf4, 0x30, 0x53, 0x84, 0x24,
        0xb1, 0x32, 0x98, 0xe6, 0xaa, 0x6f, 0xb1, 0x43,
        0xef, 0x4d, 0x59, 0xa1, 0x49, 0x46, 0x17, 0x59,
        0x97, 0x47, 0x9d, 0xbc, 0x2d, 0x1a, 0x3c, 0xd8
    };
    hmac_sha256(key, 3, data, strlen(data), digest);
    assert(memcmp(digest, expected_hmac, 32) == 0);
    printf("PASS: SHA256 and HMAC-SHA256 test vectors passed.\n");
}

static void test_curve25519(void)
{
    /* RFC 7748 Section 6.1 Test Vector */
    const uint8_t alice_priv[32] = {
        0x77, 0x07, 0x6d, 0x0a, 0x73, 0x18, 0xa5, 0x7d,
        0x3c, 0x16, 0xc1, 0x72, 0x51, 0xb2, 0x66, 0x45,
        0xdf, 0x4c, 0x2f, 0x87, 0xeb, 0xc0, 0x99, 0x2a,
        0xb1, 0x77, 0xfb, 0xa5, 0x1d, 0xb9, 0x2c, 0x2a
    };
    const uint8_t expected_alice_pub[32] = {
        0x85, 0x20, 0xf0, 0x09, 0x89, 0x30, 0xa7, 0x54,
        0x74, 0x8b, 0x7d, 0xdc, 0xb4, 0x3e, 0xf7, 0x5a,
        0x0d, 0xbf, 0x3a, 0x0d, 0x26, 0x38, 0x15, 0xe4,
        0x2b, 0x60, 0x46, 0x11, 0x81, 0x9b, 0x32, 0x4f
    };
    uint8_t alice_pub[32];
    curve25519_base(alice_pub, alice_priv);
    assert(memcmp(alice_pub, expected_alice_pub, 32) == 0);

    const uint8_t bob_priv[32] = {
        0x5d, 0xab, 0x08, 0x7e, 0x62, 0x4a, 0x8a, 0x4b,
        0x79, 0xe1, 0x7f, 0x8b, 0x83, 0x80, 0x0e, 0xe6,
        0x6f, 0x3b, 0xb1, 0x29, 0x26, 0x18, 0xb6, 0xfd,
        0x1c, 0x2f, 0x8b, 0x27, 0xff, 0x88, 0xe0, 0xeb
    };
    const uint8_t expected_bob_pub[32] = {
        0xde, 0x9e, 0xdb, 0x7d, 0x7b, 0x7d, 0xc1, 0xb4,
        0xd3, 0x5b, 0x61, 0xc2, 0xec, 0xe4, 0x35, 0x37,
        0x3f, 0x83, 0x43, 0xc8, 0x5b, 0x78, 0x67, 0x4d,
        0xad, 0xfc, 0x7e, 0x14, 0x6f, 0x88, 0x2b, 0x4f
    };
    uint8_t bob_pub[32];
    curve25519_base(bob_pub, bob_priv);
    assert(memcmp(bob_pub, expected_bob_pub, 32) == 0);

    /* Shared Secret */
    const uint8_t expected_shared[32] = {
        0x4a, 0x5d, 0x9d, 0x5b, 0xa4, 0xce, 0x2d, 0xe1,
        0x72, 0x8e, 0x3b, 0xf4, 0x80, 0x35, 0x0f, 0x25,
        0xe0, 0x7e, 0x21, 0xc9, 0x47, 0xd1, 0x9e, 0x33,
        0x76, 0xf0, 0x9b, 0x3c, 0x1e, 0x16, 0x17, 0x42
    };
    uint8_t shared_alice[32], shared_bob[32];
    curve25519_eval(shared_alice, alice_priv, bob_pub);
    curve25519_eval(shared_bob, bob_priv, alice_pub);
    assert(memcmp(shared_alice, expected_shared, 32) == 0);
    assert(memcmp(shared_bob, expected_shared, 32) == 0);
    printf("PASS: Curve25519 (X25519) RFC 7748 test vectors passed.\n");
}

static void test_aes_ctr(void)
{
    uint8_t key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                       0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    uint8_t iv[16]  = {0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
                       0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF};

    const char *plaintext = "This is a confidential SSH-2.0 packet payload test message!";
    size_t len = strlen(plaintext);

    uint8_t ciphertext[128] = {0};
    uint8_t decrypted[128] = {0};

    aes_ctr_ctx_t enc, dec;
    assert(aes_ctr_init(&enc, key, 128, iv) == 0);
    assert(aes_ctr_init(&dec, key, 128, iv) == 0);

    aes_ctr_crypt(&enc, (const uint8_t *)plaintext, ciphertext, len);
    assert(memcmp(plaintext, ciphertext, len) != 0);

    aes_ctr_crypt(&dec, ciphertext, decrypted, len);
    decrypted[len] = '\0';
    assert(strcmp((const char *)decrypted, plaintext) == 0);

    printf("PASS: AES-128-CTR round-trip encryption passed.\n");
}

static void test_kex_negotiation(void)
{
    ssh_kex_init_t client_kex, server_kex;
    kex_init_default(&client_kex, false);
    kex_init_default(&server_kex, true);

    ssh_kex_proposal_t chosen;
    assert(kex_negotiate(&client_kex, &server_kex, &chosen) == 0);
    assert(strcmp(chosen.kex, "curve25519-sha256") == 0);
    assert(strcmp(chosen.cipher_c2s, "aes128-ctr") == 0);
    assert(strcmp(chosen.mac_c2s, "hmac-sha2-256") == 0);

    kex_proposal_free(&chosen);
    kex_init_free(&client_kex);
    kex_init_free(&server_kex);
    printf("PASS: KEXINIT proposal negotiation passed.\n");
}

int main(void)
{
    printf("Running Phase 2 Cryptography & KEX Tests...\n");
    test_sha256();
    test_curve25519();
    test_aes_ctr();
    test_kex_negotiation();
    return 0;
}
