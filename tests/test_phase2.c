#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/sha256.h"
#include "../src/sha512.h"
#include "../src/curve25519.h"
#include "../src/ed25519.h"
#include "../src/aes.h"
#include "../src/base64.h"
#include "../src/rand.h"
#include "../src/kex.h"

static void hex_to_bytes(uint8_t *dst, const char *hex)
{
    size_t n = strlen(hex) / 2;
    for (size_t i = 0; i < n; i++) {
        unsigned v = 0;
        if (sscanf(hex + 2 * i, "%2x", &v) != 1) {
            printf("FAIL: bad hex constant\n");
            exit(1);
        }
        dst[i] = (uint8_t)v;
    }
}

static void expect_hex(const uint8_t *got, size_t got_len, const char *hex_expected,
                       const char *what)
{
    uint8_t expected[128];
    assert(strlen(hex_expected) / 2 == got_len);
    hex_to_bytes(expected, hex_expected);
    if (memcmp(got, expected, got_len) != 0) {
        printf("FAIL: %s\n  got:      ", what);
        for (size_t i = 0; i < got_len; i++) printf("%02x", got[i]);
        printf("\n  expected: %s\n", hex_expected);
        exit(1);
    }
}

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

static void test_sha512(void)
{
    uint8_t digest[64];
    sha512_ctx_t ctx;

    /* FIPS 180-4: SHA-512("abc") */
    sha512("abc", 3, digest);
    expect_hex(digest, 64,
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
        "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
        "SHA-512(\"abc\")");

    /* SHA-512("") — empty message */
    sha512("", 0, digest);
    expect_hex(digest, 64,
        "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
        "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e",
        "SHA-512(\"\")");

    /* 112-byte NIST example, fed incrementally in two chunks */
    const char *msg112 =
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
        "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
    assert(strlen(msg112) == 112);
    sha512_init(&ctx);
    sha512_update(&ctx, msg112, 63);
    sha512_update(&ctx, msg112 + 63, 49);
    sha512_final(&ctx, digest);
    expect_hex(digest, 64,
        "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
        "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909",
        "SHA-512(112-byte NIST message)");

    /* One million 'a' — exercises multi-block streaming */
    uint8_t block[1000];
    memset(block, 'a', sizeof(block));
    sha512_init(&ctx);
    for (int i = 0; i < 1000; i++) {
        sha512_update(&ctx, block, sizeof(block));
    }
    sha512_final(&ctx, digest);
    expect_hex(digest, 64,
        "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973eb"
        "de0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b",
        "SHA-512(1 million 'a')");

    printf("PASS: SHA-512 FIPS 180-4 test vectors passed.\n");
}

static void test_ed25519(void)
{
    uint8_t seed[32], pub[32], sig[64];
    uint8_t msg[3] = { 0x72, 0xaf, 0x82 };

    /* ── RFC 8032 §7.1 TEST 1: empty message ── */
    hex_to_bytes(seed, "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
    assert(ed25519_pubkey_from_seed(pub, seed) == 0);
    expect_hex(pub, 32,
        "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a",
        "Ed25519 TEST1 public key");
    assert(ed25519_sign(sig, seed, (const uint8_t *)"", 0) == 0);
    expect_hex(sig, 64,
        "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
        "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b",
        "Ed25519 TEST1 signature");
    assert(ed25519_verify(pub, sig, (const uint8_t *)"", 0) == 0);

    /* ── RFC 8032 §7.1 TEST 2: one-byte message 0x72 ── */
    hex_to_bytes(seed, "4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb");
    assert(ed25519_pubkey_from_seed(pub, seed) == 0);
    expect_hex(pub, 32,
        "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c",
        "Ed25519 TEST2 public key");
    assert(ed25519_sign(sig, seed, msg, 1) == 0);
    expect_hex(sig, 64,
        "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
        "085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00",
        "Ed25519 TEST2 signature");
    assert(ed25519_verify(pub, sig, msg, 1) == 0);

    /* ── RFC 8032 §7.1 TEST 3: two-byte message af82 ── */
    hex_to_bytes(seed, "c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7");
    assert(ed25519_pubkey_from_seed(pub, seed) == 0);
    expect_hex(pub, 32,
        "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025",
        "Ed25519 TEST3 public key");
    assert(ed25519_sign(sig, seed, msg + 1, 2) == 0);
    expect_hex(sig, 64,
        "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac"
        "18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a",
        "Ed25519 TEST3 signature");
    assert(ed25519_verify(pub, sig, msg + 1, 2) == 0);

    /* ── Negative cases ── */
    uint8_t bad[64];
    memcpy(bad, sig, 64);
    bad[0] ^= 0x01;                                  /* corrupted R */
    assert(ed25519_verify(pub, bad, msg + 1, 2) != 0);
    assert(ed25519_verify(pub, sig, msg + 1, 1) != 0); /* wrong message length */
    memcpy(bad, pub, 32);
    bad[0] ^= 0x01;                                  /* corrupted public key */
    assert(ed25519_verify(bad, sig, msg + 1, 2) != 0);

    /* S = L exactly (first 4 LE words of L) must be rejected as non-canonical */
    memcpy(bad, sig, 64);
    hex_to_bytes(bad + 32,
        "edd3f55c1a631258d69cf7a2def90e1400000000000000000000000000000010");
    assert(ed25519_verify(pub, bad, msg + 1, 2) != 0);

    /* ── Fresh random keypair signs and verifies ── */
    uint8_t rpub[32], rseed[32];
    ed25519_keypair(rpub, rseed);
    assert(ed25519_sign(sig, rseed, msg, 3) == 0);
    assert(ed25519_verify(rpub, sig, msg, 3) == 0);
    assert(ed25519_verify(rpub, sig, msg, 2) != 0);

    printf("PASS: Ed25519 RFC 8032 test vectors + negative cases passed.\n");
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
        0x0d, 0xbf, 0x3a, 0x0d, 0x26, 0x38, 0x1a, 0xf4,
        0xeb, 0xa4, 0xa9, 0x8e, 0xaa, 0x9b, 0x4e, 0x6a
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

static void test_aes_block(void)
{
    /* FIPS-197 Appendix C: single-block encryption vectors.
     * Plaintext: 00112233445566778899aabbccddeeff */
    uint8_t pt[16], out[16];
    hex_to_bytes(pt, "00112233445566778899aabbccddeeff");

    aes_ctx_t ctx;

    /* C.1 — AES-128, key 000102...0f */
    uint8_t key128[16];
    hex_to_bytes(key128, "000102030405060708090a0b0c0d0e0f");
    assert(aes_set_encrypt_key(&ctx, key128, 128) == 0);
    aes_encrypt_block(&ctx, pt, out);
    expect_hex(out, 16, "69c4e0d86a7b0430d8cdb78070b4c55a", "AES-128 FIPS-197 C.1");

    /* C.2 — AES-192, key 000102...17 */
    uint8_t key192[24];
    hex_to_bytes(key192, "000102030405060708090a0b0c0d0e0f1011121314151617");
    assert(aes_set_encrypt_key(&ctx, key192, 192) == 0);
    aes_encrypt_block(&ctx, pt, out);
    expect_hex(out, 16, "dda97ca4864cdfe06eaf70a0ec0d7191", "AES-192 FIPS-197 C.2");

    /* C.3 — AES-256, key 000102...1f */
    uint8_t key256[32];
    hex_to_bytes(key256, "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    assert(aes_set_encrypt_key(&ctx, key256, 256) == 0);
    aes_encrypt_block(&ctx, pt, out);
    expect_hex(out, 16, "8ea2b7ca516745bfeafc49904b496089", "AES-256 FIPS-197 C.3");

    /* AES-256-CTR stream: init twice with same key/iv, split blocks must equal
     * a single-shot encryption of the concatenated data. */
    uint8_t iv[16], big[40], ct_a[40], ct_b[40];
    hex_to_bytes(iv, "00000000000000000000000000000000");
    memset(big, 0x5A, sizeof(big));

    aes_ctr_ctx_t a, b;
    assert(aes_ctr_init(&a, key256, 256, iv) == 0);
    aes_ctr_crypt(&a, big, ct_a, sizeof(big));

    assert(aes_ctr_init(&b, key256, 256, iv) == 0);
    aes_ctr_crypt(&b, big, ct_b, 7);          /* partial first block */
    aes_ctr_crypt(&b, big + 7, ct_b + 7, 16); /* aligned second block */
    aes_ctr_crypt(&b, big + 23, ct_b + 23, 17); /* final partial */
    assert(memcmp(ct_a, ct_b, sizeof(big)) == 0);

    printf("PASS: AES FIPS-197 block + CTR streaming vectors passed.\n");
}

static void test_base64(void)
{
    char enc[96];
    uint8_t dec[64];

    /* RFC 4648 §10 test vectors */
    assert(base64_encode(enc, sizeof(enc), (const uint8_t *)"", 0) == 0);
    assert(strcmp(enc, "") == 0);
    assert(base64_encode(enc, sizeof(enc), (const uint8_t *)"f", 1) == 4);
    assert(strcmp(enc, "Zg==") == 0);
    assert(base64_encode(enc, sizeof(enc), (const uint8_t *)"fo", 2) == 4);
    assert(strcmp(enc, "Zm8=") == 0);
    assert(base64_encode(enc, sizeof(enc), (const uint8_t *)"foo", 3) == 4);
    assert(strcmp(enc, "Zm9v") == 0);
    assert(base64_encode(enc, sizeof(enc), (const uint8_t *)"foob", 4) == 8);
    assert(strcmp(enc, "Zm9vYg==") == 0);
    assert(base64_encode(enc, sizeof(enc), (const uint8_t *)"fooba", 5) == 8);
    assert(strcmp(enc, "Zm9vYmE=") == 0);
    assert(base64_encode(enc, sizeof(enc), (const uint8_t *)"foobar", 6) == 8);
    assert(strcmp(enc, "Zm9vYmFy") == 0);

    /* Decode round-trip including newline tolerance */
    assert(base64_decode(dec, sizeof(dec), "Zm9vYmFy") == 6);
    assert(memcmp(dec, "foobar", 6) == 0);
    assert(base64_decode(dec, sizeof(dec), "Zm9v\r\nYmFy\n") == 6);
    assert(memcmp(dec, "foobar", 6) == 0);
    assert(base64_decode(dec, sizeof(dec), "Zg==") == 1);
    assert(dec[0] == 'f');

    /* Reject malformed input */
    assert(base64_decode(dec, sizeof(dec), "Zm9v!") < 0);
    assert(base64_decode(dec, sizeof(dec), "Zg==Zg==") < 0);

    /* Binary round-trip through all lengths 0..47 */
    uint8_t bin[47], back[47];
    for (int i = 0; i < 47; i++) bin[i] = (uint8_t)(i * 7 + 3);
    for (int len = 0; len <= 47; len++) {
        int clen = base64_encode(enc, sizeof(enc), bin, (size_t)len);
        assert(clen >= 0);
        int blen = base64_decode(back, sizeof(back), enc);
        assert(blen == len);
        assert(memcmp(bin, back, (size_t)len) == 0);
    }

    printf("PASS: base64 RFC 4648 vectors and round-trips passed.\n");
}

static void test_rand(void)
{
    uint8_t a[32], b[32];
    assert(rand_bytes(a, sizeof(a)) == 0);
    assert(rand_bytes(b, sizeof(b)) == 0);

    /* Two draws must never collide (astronomically unlikely if real entropy) */
    assert(memcmp(a, b, sizeof(a)) != 0);

    /* Not all-zero */
    uint8_t acc = 0;
    for (size_t i = 0; i < sizeof(a); i++) acc |= a[i];
    assert(acc != 0);

    printf("PASS: OS entropy source returned independent random bytes.\n");
}

static void test_kex_negotiation(void)
{
    ssh_kex_init_t client_kex, server_kex;
    kex_init_default(&client_kex, false);
    kex_init_default(&server_kex, true);

    ssh_kex_proposal_t chosen;
    assert(kex_negotiate(&client_kex, &server_kex, &chosen) == 0);
    assert(strcmp(chosen.kex, "curve25519-sha256") == 0);
    assert(strcmp(chosen.host_key, "ssh-ed25519") == 0);
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
    test_rand();
    test_sha256();
    test_sha512();
    test_curve25519();
    test_ed25519();
    test_aes_ctr();
    test_aes_block();
    test_base64();
    test_kex_negotiation();
    printf("All Phase 2 tests passed.\n");
    return 0;
}
