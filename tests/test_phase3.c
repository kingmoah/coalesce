/* test_phase3.c — session layer tests: identification exchange, binary
 * packet framing, encrypted framing and MAC enforcement (RFC 4253). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/net.h"
#include "../src/ssh.h"
#include "../src/buffer.h"
#include "../src/session.h"
#include "../src/rand.h"

/* ── Helpers ───────────────────────────────────────────────────── */

/* Create a connected socket pair over 127.0.0.1 (no threads needed:
 * connect() completes into the accept backlog before accept() is called). */
static int make_pair(net_socket_t *cli, net_socket_t *srv)
{
    for (int port = 42100; port < 42200; port++) {
        net_socket_t l = net_listen("127.0.0.1", port);
        if (!NET_IS_VALID(l)) continue;

        net_socket_t c = net_connect("127.0.0.1", port);
        if (!NET_IS_VALID(c)) {
            net_close(l);
            continue;
        }

        struct net_addr addr = {0};
        net_socket_t a = net_accept(l, &addr);
        net_close(l);
        if (!NET_IS_VALID(a)) {
            net_close(c);
            continue;
        }

        *cli = c;
        *srv = a;
        return 0;
    }
    return -1;
}

#define CHECK_OK(line)  assert(session_ident_check((line), strlen(line)) == 0)
#define CHECK_BAD(line) assert(session_ident_check((line), strlen(line)) != 0)

/* ── Identification string validation (RFC 4253 §4.2) ──────────── */

static void test_ident_check(void)
{
    /* valid */
    CHECK_OK("SSH-2.0-OpenSSH_9.6\r\n");
    CHECK_OK("SSH-1.99-compat\r\n");
    CHECK_OK("SSH-2.0-coalesce_0.0.1\r\n");
    CHECK_OK("SSH-2.0-software with comments here\r\n");
    CHECK_OK("SSH-2.0-coalesce_test"); /* no trailing CRLF is acceptable */

    /* invalid */
    CHECK_BAD("SSH-1.5-legacy\r\n");       /* old protocol */
    CHECK_BAD("SSH-2.0-\r\n");             /* empty softwareversion */
    CHECK_BAD("SSH-2.0-soft-ware\r\n");    /* '-' inside softwareversion */
    CHECK_BAD("SSH-2.0-soft\tware\r\n");   /* tab not printable in softwareversion */
    CHECK_BAD("SSH-2.0-sp ace\x01\r\n");   /* control char in comments */
    CHECK_BAD("HTTP/1.1 200 OK\r\n");      /* not an SSH banner */
    CHECK_BAD("ssh-2.0-lowercase\r\n");    /* prefix is case sensitive */

    /* length boundary: 255 bytes including CR LF is OK, 256 is not */
    char line[300];
    memset(line, 'a', sizeof line);
    memcpy(line, "SSH-2.0-", 8);
    line[253] = '\r';
    line[254] = '\n';
    assert(session_ident_check(line, 255) == 0);

    line[254] = 'a';
    line[255] = '\r';
    line[256] = '\n';
    assert(session_ident_check(line, 257) != 0);
}

/* ── Key installation validation ───────────────────────────────── */

static void test_set_keys_validation(void)
{
    ssh_direction_t d;
    memset(&d, 0, sizeof d);

    uint8_t iv[16] = {0};
    uint8_t key32[32] = {0};
    uint8_t mac[32] = {0};

    /* unsupported cipher */
    assert(session_set_keys(&d, "aes256-cbc", iv, 16, key32, 32,
                            "hmac-sha2-256", mac, 32) != 0);
    /* wrong IV length */
    assert(session_set_keys(&d, "aes256-ctr", iv, 8, key32, 32,
                            "hmac-sha2-256", mac, 32) != 0);
    /* wrong key length for aes256-ctr */
    assert(session_set_keys(&d, "aes256-ctr", iv, 16, key32, 16,
                            "hmac-sha2-256", mac, 32) != 0);
    /* unsupported MAC */
    assert(session_set_keys(&d, "aes256-ctr", iv, 16, key32, 32,
                            "hmac-sha1", mac, 32) != 0);

    /* valid: aes256-ctr + hmac-sha2-256 */
    assert(session_set_keys(&d, "aes256-ctr", iv, 16, key32, 32,
                            "hmac-sha2-256", mac, 32) == 0);
    assert(d.mac_key_len == 32);
    assert(!d.encrypted);

    /* valid: aes128-ctr + none */
    assert(session_set_keys(&d, "aes128-ctr", iv, 16, key32, 16,
                            "none", NULL, 0) == 0);
    assert(d.mac_key_len == 0);
    session_activate(&d);
    assert(d.encrypted);
}

/* ── Live banner exchange over loopback ────────────────────────── */

static void test_ident_exchange(void)
{
    net_socket_t cli, srv;
    ssh_session_t s;
    size_t expect = strlen(IDENT_STRING) - 2; /* CRLF stripped */

    /* The peer banner must be on the wire before the exchange call,
     * because both sides write-then-read and this test is single
     * threaded (no real handshake concurrency). */
    assert(make_pair(&cli, &srv) == 0);
    assert(net_write_full(srv, IDENT_STRING, strlen(IDENT_STRING)) ==
           (ssize_t)strlen(IDENT_STRING));
    session_init(&s, cli);
    assert(session_exchange_ident(&s, SSH_ROLE_CLIENT) == 0);
    assert(s.v_c_len == expect && memcmp(s.v_c, IDENT_STRING, expect) == 0);
    assert(s.v_s_len == expect && memcmp(s.v_s, IDENT_STRING, expect) == 0);
    session_free(&s);
    net_close(cli);
    net_close(srv);

    /* same for the server role */
    assert(make_pair(&cli, &srv) == 0);
    assert(net_write_full(cli, IDENT_STRING, strlen(IDENT_STRING)) ==
           (ssize_t)strlen(IDENT_STRING));
    session_init(&s, srv);
    assert(session_exchange_ident(&s, SSH_ROLE_SERVER) == 0);
    assert(s.v_c_len == expect && memcmp(s.v_c, IDENT_STRING, expect) == 0);
    assert(s.v_s_len == expect && memcmp(s.v_s, IDENT_STRING, expect) == 0);
    session_free(&s);
    net_close(cli);
    net_close(srv);
}

static void test_ident_skips_prelines(void)
{
    net_socket_t cli, srv;
    assert(make_pair(&cli, &srv) == 0);

    /* the peer sends junk lines before its version banner */
    const char *pre = "greetings from a proxy\r\nstill not ssh\r\n";
    assert(net_write_full(cli, pre, strlen(pre)) == (ssize_t)strlen(pre));
    assert(net_write_full(cli, IDENT_STRING, strlen(IDENT_STRING)) ==
           (ssize_t)strlen(IDENT_STRING));

    ssh_session_t ss;
    session_init(&ss, srv);
    assert(session_exchange_ident(&ss, SSH_ROLE_SERVER) == 0);

    size_t expect = strlen(IDENT_STRING) - 2;
    assert(ss.v_c_len == expect && memcmp(ss.v_c, IDENT_STRING, expect) == 0);

    session_free(&ss);
    net_close(cli);
    net_close(srv);
}

static void test_ident_rejects_bad(void)
{
    ssh_session_t ss;

    /* SSH-1.5 must be rejected */
    net_socket_t cli, srv;
    assert(make_pair(&cli, &srv) == 0);
    const char *bad = "SSH-1.5-legacy\r\n";
    assert(net_write_full(cli, bad, strlen(bad)) == (ssize_t)strlen(bad));
    session_init(&ss, srv);
    assert(session_exchange_ident(&ss, SSH_ROLE_SERVER) != 0);
    session_free(&ss);
    net_close(cli);
    net_close(srv);

    /* a line longer than 255 bytes is never accepted */
    assert(make_pair(&cli, &srv) == 0);
    char big[400];
    memset(big, 'x', sizeof big);
    assert(net_write_full(cli, big, sizeof big) == (ssize_t)sizeof big);
    session_init(&ss, srv);
    assert(session_exchange_ident(&ss, SSH_ROLE_SERVER) != 0);
    session_free(&ss);
    net_close(cli);
    net_close(srv);
}

/* ── Plaintext binary packet framing ───────────────────────────── */

static void test_framing_plaintext(void)
{
    net_socket_t cli, srv;
    assert(make_pair(&cli, &srv) == 0);

    ssh_session_t cs, ss;
    session_init(&cs, cli);
    session_init(&ss, srv);

    size_t sizes[] = {1, 5, 100, 30000};
    for (size_t t = 0; t < 4; t++) {
        size_t n = sizes[t];
        uint8_t *payload = malloc(n);
        assert(payload != NULL);
        for (size_t i = 0; i < n; i++) payload[i] = (uint8_t)(i * 31 + t);

        assert(session_send(&cs, payload, n) == 0);
        assert(cs.out.seq == (uint32_t)(t + 1));

        ssh_buf_t got;
        buf_init(&got, 64);
        assert(session_recv(&ss, &got) == 0);
        assert(ss.in.seq == (uint32_t)(t + 1));
        assert(buf_len(&got) == n);
        assert(memcmp(buf_data(&got), payload, n) == 0);
        buf_free(&got);
        free(payload);
    }

    /* the reverse direction works on the same connection */
    uint8_t pong[42];
    for (size_t i = 0; i < sizeof pong; i++) pong[i] = (uint8_t)(255 - i);
    assert(session_send(&ss, pong, sizeof pong) == 0);
    assert(ss.out.seq == 1);

    ssh_buf_t got;
    buf_init(&got, 64);
    assert(session_recv(&cs, &got) == 0);
    assert(cs.in.seq == 1);
    assert(buf_len(&got) == sizeof pong);
    assert(memcmp(buf_data(&got), pong, sizeof pong) == 0);
    buf_free(&got);

    session_free(&cs);
    session_free(&ss);
    net_close(cli);
    net_close(srv);
}

/* ── Encrypted framing ─────────────────────────────────────────── */

static void test_framing_encrypted(void)
{
    net_socket_t cli, srv;
    assert(make_pair(&cli, &srv) == 0);

    ssh_session_t cs, ss;
    session_init(&cs, cli);
    session_init(&ss, srv);

    uint8_t iv1[16], iv2[16], k1[32], k2[32], m1[32], m2[32];
    assert(rand_bytes(iv1, sizeof iv1) == 0);
    assert(rand_bytes(iv2, sizeof iv2) == 0);
    assert(rand_bytes(k1, sizeof k1) == 0);
    assert(rand_bytes(k2, sizeof k2) == 0);
    assert(rand_bytes(m1, sizeof m1) == 0);
    assert(rand_bytes(m2, sizeof m2) == 0);

    /* client -> server keys */
    assert(session_set_keys(&cs.out, "aes256-ctr", iv1, 16, k1, 32,
                            "hmac-sha2-256", m1, 32) == 0);
    assert(session_set_keys(&ss.in, "aes256-ctr", iv1, 16, k1, 32,
                            "hmac-sha2-256", m1, 32) == 0);
    /* server -> client keys */
    assert(session_set_keys(&ss.out, "aes256-ctr", iv2, 16, k2, 32,
                            "hmac-sha2-256", m2, 32) == 0);
    assert(session_set_keys(&cs.in, "aes256-ctr", iv2, 16, k2, 32,
                            "hmac-sha2-256", m2, 32) == 0);

    session_activate(&cs.out);
    session_activate(&ss.in);
    session_activate(&ss.out);
    session_activate(&cs.in);

    size_t sizes[] = {1, 15, 16, 17, 64, 1000};
    for (size_t t = 0; t < 6; t++) {
        size_t n = sizes[t];
        uint8_t *payload = malloc(n);
        assert(payload != NULL);
        for (size_t i = 0; i < n; i++) payload[i] = (uint8_t)(i * 7 + t + 1);

        /* forward */
        assert(session_send(&cs, payload, n) == 0);
        ssh_buf_t got;
        buf_init(&got, 64);
        assert(session_recv(&ss, &got) == 0);
        assert(buf_len(&got) == n);
        assert(memcmp(buf_data(&got), payload, n) == 0);
        buf_free(&got);

        /* and back */
        assert(session_send(&ss, payload, n) == 0);
        buf_init(&got, 64);
        assert(session_recv(&cs, &got) == 0);
        assert(buf_len(&got) == n);
        assert(memcmp(buf_data(&got), payload, n) == 0);
        buf_free(&got);

        free(payload);
    }
    assert(cs.out.seq == 6 && ss.in.seq == 6);
    assert(ss.out.seq == 6 && cs.in.seq == 6);

    session_free(&cs);
    session_free(&ss);
    net_close(cli);
    net_close(srv);
}

static void test_framing_encrypted_nomac(void)
{
    net_socket_t cli, srv;
    assert(make_pair(&cli, &srv) == 0);

    ssh_session_t cs, ss;
    session_init(&cs, cli);
    session_init(&ss, srv);

    uint8_t iv[16], k[16];
    assert(rand_bytes(iv, sizeof iv) == 0);
    assert(rand_bytes(k, sizeof k) == 0);

    assert(session_set_keys(&cs.out, "aes128-ctr", iv, 16, k, 16,
                            "none", NULL, 0) == 0);
    assert(session_set_keys(&ss.in, "aes128-ctr", iv, 16, k, 16,
                            "none", NULL, 0) == 0);
    assert(session_set_keys(&ss.out, "aes128-ctr", iv, 16, k, 16,
                            "none", NULL, 0) == 0);
    assert(session_set_keys(&cs.in, "aes128-ctr", iv, 16, k, 16,
                            "none", NULL, 0) == 0);

    session_activate(&cs.out);
    session_activate(&ss.in);
    session_activate(&ss.out);
    session_activate(&cs.in);

    uint8_t p1[3] = {1, 2, 3};
    uint8_t p2[33];
    for (size_t i = 0; i < sizeof p2; i++) p2[i] = (uint8_t)i;

    assert(session_send(&cs, p1, sizeof p1) == 0);
    ssh_buf_t got;
    buf_init(&got, 64);
    assert(session_recv(&ss, &got) == 0);
    assert(buf_len(&got) == sizeof p1 && memcmp(buf_data(&got), p1, sizeof p1) == 0);
    buf_free(&got);

    assert(session_send(&ss, p2, sizeof p2) == 0);
    buf_init(&got, 64);
    assert(session_recv(&cs, &got) == 0);
    assert(buf_len(&got) == sizeof p2 && memcmp(buf_data(&got), p2, sizeof p2) == 0);
    buf_free(&got);

    session_free(&cs);
    session_free(&ss);
    net_close(cli);
    net_close(srv);
}

/* ── MAC enforcement ───────────────────────────────────────────── */

static void test_mac_failure(void)
{
    net_socket_t cli, srv;
    assert(make_pair(&cli, &srv) == 0);

    ssh_session_t cs, ss;
    session_init(&cs, cli);
    session_init(&ss, srv);

    uint8_t iv[16], k[32], m[32];
    assert(rand_bytes(iv, sizeof iv) == 0);
    assert(rand_bytes(k, sizeof k) == 0);
    assert(rand_bytes(m, sizeof m) == 0);

    assert(session_set_keys(&cs.out, "aes256-ctr", iv, 16, k, 32,
                            "hmac-sha2-256", m, 32) == 0);
    assert(session_set_keys(&ss.in, "aes256-ctr", iv, 16, k, 32,
                            "hmac-sha2-256", m, 32) == 0);
    session_activate(&cs.out);
    session_activate(&ss.in);

    uint8_t p[20];
    for (size_t i = 0; i < sizeof p; i++) p[i] = (uint8_t)(i + 1);

    /* first packet verifies fine */
    assert(session_send(&cs, p, sizeof p) == 0);
    ssh_buf_t got;
    buf_init(&got, 64);
    assert(session_recv(&ss, &got) == 0);
    buf_free(&got);

    /* tamper with the receiver's MAC key: next packet must be rejected */
    ss.in.mac_key[0] ^= 0xFF;
    assert(session_send(&cs, p, sizeof p) == 0);
    buf_init(&got, 64);
    assert(session_recv(&ss, &got) == -1);
    buf_free(&got);

    session_free(&cs);
    session_free(&ss);
    net_close(cli);
    net_close(srv);
}

/* ── Malformed plaintext packets are rejected ──────────────────── */

static void test_recv_malformed_plaintext(void)
{
    net_socket_t cli, srv;
    ssh_session_t ss;
    ssh_buf_t got;

    /* packet_length = 5, below the 6-byte minimum */
    assert(make_pair(&cli, &srv) == 0);
    session_init(&ss, srv);
    uint8_t short_len[4] = {0, 0, 0, 5};
    assert(net_write_full(cli, short_len, 4) == 4);
    buf_init(&got, 64);
    assert(session_recv(&ss, &got) == -1);
    buf_free(&got);
    session_free(&ss);
    net_close(cli);
    net_close(srv);

    /* padding_length = 2, below the 4-byte minimum */
    assert(make_pair(&cli, &srv) == 0);
    session_init(&ss, srv);
    uint8_t bad_pad[16] = {0};
    bad_pad[3] = 12;   /* packet_length */
    bad_pad[4] = 2;    /* padding_length */
    assert(net_write_full(cli, bad_pad, 16) == 16);
    buf_init(&got, 64);
    assert(session_recv(&ss, &got) == -1);
    buf_free(&got);
    session_free(&ss);
    net_close(cli);
    net_close(srv);

    /* padding consumes everything: the payload would be empty */
    assert(make_pair(&cli, &srv) == 0);
    session_init(&ss, srv);
    uint8_t no_payload[16] = {0};
    no_payload[3] = 12;
    no_payload[4] = 11; /* 16 - 4 - 1 - 11 = 0 payload bytes */
    assert(net_write_full(cli, no_payload, 16) == 16);
    buf_init(&got, 64);
    assert(session_recv(&ss, &got) == -1);
    buf_free(&got);
    session_free(&ss);
    net_close(cli);
    net_close(srv);
}

int main(void)
{
    printf("Running Phase 3 Session Layer Tests...\n");

    assert(net_init() == 0);

    test_ident_check();
    printf("PASS: identification string validation\n");

    test_set_keys_validation();
    printf("PASS: session_set_keys validation\n");

    test_ident_exchange();
    printf("PASS: version banner exchange over loopback\n");

    test_ident_skips_prelines();
    printf("PASS: pre-banner lines are skipped\n");

    test_ident_rejects_bad();
    printf("PASS: bad version banners rejected\n");

    test_framing_plaintext();
    printf("PASS: plaintext packet framing\n");

    test_framing_encrypted();
    printf("PASS: encrypted framing (aes256-ctr + hmac-sha2-256)\n");

    test_framing_encrypted_nomac();
    printf("PASS: encrypted framing without MAC\n");

    test_mac_failure();
    printf("PASS: MAC failure detected\n");

    test_recv_malformed_plaintext();
    printf("PASS: malformed plaintext packets rejected\n");

    printf("All Phase 3 tests passed.\n");
    net_shutdown();
    return 0;
}
