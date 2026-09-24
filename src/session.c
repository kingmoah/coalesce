/* session.c — transport session state and the binary packet protocol. */
#include "session.h"
#include "sha256.h"
#include "rand.h"
#include "ssh.h"
#include <stdlib.h>
#include <string.h>

#define SSH_CIPHER_BLOCK 16   /* AES block size once encryption is active */
#define SSH_PLAIN_BLOCK  8    /* minimum block size per RFC 4253 §6 */

/* ── Lifecycle ─────────────────────────────────────────────────── */

void session_init(ssh_session_t *s, net_socket_t sock)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->sock = sock;
}

void session_free(ssh_session_t *s)
{
    if (!s) return;
    free(s->i_c);
    free(s->i_s);
    s->i_c = NULL;
    s->i_s = NULL;
    s->i_c_len = 0;
    s->i_s_len = 0;
}

/* ── Identification exchange (RFC 4253 §4.2) ───────────────────── */

static size_t ident_strip_crlf(const char *line, size_t len)
{
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        len--;
    }
    return len;
}

int session_ident_check(const char *line, size_t len)
{
    if (!line) return -1;

    len = ident_strip_crlf(line, len);
    if (len == 0 || len > SSH_IDENT_MAX - 2) return -1; /* +CRLF must fit 255 */

    /* "SSH-" prefix */
    if (len < 8 || memcmp(line, "SSH-", 4) != 0) return -1;

    /* protocol version: 2.0, or 1.99 which must be treated as 2.0 */
    size_t vlen;
    if (memcmp(line + 4, "2.0", 3) == 0) {
        vlen = 3;
    } else if (len >= 9 && memcmp(line + 4, "1.99", 4) == 0) {
        vlen = 4;
    } else {
        return -1;
    }

    size_t pos = 4 + vlen;
    if (pos >= len || line[pos] != '-') return -1;
    pos++; /* start of softwareversion */
    if (pos >= len) return -1; /* empty softwareversion */

    /* softwareversion: printable US-ASCII without whitespace or '-' */
    size_t end = pos;
    while (end < len && line[end] != ' ') {
        uint8_t c = (uint8_t)line[end];
        if (c < 0x21 || c > 0x7E || c == '-') return -1;
        end++;
    }

    /* optional " SP comments": printable US-ASCII only */
    for (size_t i = end; i < len; i++) {
        uint8_t c = (uint8_t)line[i];
        if (c < 0x20 || c > 0x7E) return -1;
    }
    return 0;
}

int session_exchange_ident(ssh_session_t *s, ssh_role_t role)
{
    if (!s) return -1;

    /* 1. send our identification string (always "SSH-" first) */
    size_t own_len = sizeof(IDENT_STRING) - 1;
    if (net_write_full(s->sock, IDENT_STRING, own_len) != (ssize_t)own_len) {
        return -1;
    }

    own_len = ident_strip_crlf(IDENT_STRING, own_len);
    if (own_len > SSH_IDENT_MAX - 2) return -1;
    if (role == SSH_ROLE_CLIENT) {
        memcpy(s->v_c, IDENT_STRING, own_len);
        s->v_c[own_len] = '\0';
        s->v_c_len = own_len;
    } else {
        memcpy(s->v_s, IDENT_STRING, own_len);
        s->v_s[own_len] = '\0';
        s->v_s_len = own_len;
    }

    /* 2. read the peer's identification string.  Either side may send
     *    additional lines before it (RFC 4253 §4.2); they must be skipped,
     *    but a peer that never produces a version line is cut loose. */
    for (int attempt = 0; attempt < 50; attempt++) {
        char line[SSH_IDENT_MAX + 2];
        ssize_t n = net_read_line(s->sock, line, sizeof(line));
        if (n <= 0) return -1;
        if (line[n - 1] != '\n') return -1;  /* line longer than the limit */

        if (strncmp(line, "SSH-", 4) != 0) continue; /* pre-banner text */
        if (session_ident_check(line, (size_t)n) != 0) return -1;

        size_t clen = ident_strip_crlf(line, (size_t)n);
        char  *dst     = (role == SSH_ROLE_CLIENT) ? s->v_s : s->v_c;
        size_t *dst_len = (role == SSH_ROLE_CLIENT) ? &s->v_s_len : &s->v_c_len;
        memcpy(dst, line, clen);
        dst[clen] = '\0';
        *dst_len = clen;
        return 0;
    }
    return -1; /* too many junk lines */
}

/* ── Key installation ──────────────────────────────────────────── */

int session_set_keys(ssh_direction_t *dir, const char *cipher_name,
                     const uint8_t *iv, size_t iv_len,
                     const uint8_t *key, size_t key_len,
                     const char *mac_name,
                     const uint8_t *mac_key, size_t mac_key_len)
{
    if (!dir || !cipher_name || !iv || !key || !mac_name) return -1;

    int key_bits;
    if (strcmp(cipher_name, "aes128-ctr") == 0) {
        key_bits = 128;
    } else if (strcmp(cipher_name, "aes256-ctr") == 0) {
        key_bits = 256;
    } else {
        return -1; /* unsupported cipher */
    }

    if (iv_len != SSH_CIPHER_BLOCK) return -1;
    if (key_len != (size_t)(key_bits / 8)) return -1;
    if (aes_ctr_init(&dir->ctr, key, key_bits, iv) != 0) return -1;

    if (strcmp(mac_name, "hmac-sha2-256") == 0) {
        if (!mac_key || mac_key_len == 0 || mac_key_len > SSH_MAC_KEY_MAX) {
            return -1;
        }
        memcpy(dir->mac_key, mac_key, mac_key_len);
        dir->mac_key_len = mac_key_len;
    } else if (strcmp(mac_name, "none") == 0) {
        dir->mac_key_len = 0;
    } else {
        return -1; /* unsupported MAC */
    }
    return 0;
}

void session_activate(ssh_direction_t *dir)
{
    if (!dir) return;
    dir->encrypted = true;
}

/* ── Packet protection helpers ─────────────────────────────────── */

static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v);
}

static uint32_t get_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int ct_equal(const uint8_t *a, const uint8_t *b, size_t n)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < n; i++) diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0;
}

/* ── Binary packet protocol (RFC 4253 §6) ──────────────────────── */

int session_send(ssh_session_t *s, const uint8_t *payload, size_t len)
{
    if (!s || !payload || len == 0 || len > MAX_PAYLOAD_SIZE) return -1;

    ssh_direction_t *d = &s->out;
    size_t block = d->encrypted ? SSH_CIPHER_BLOCK : SSH_PLAIN_BLOCK;

    /* total (packet_length || padding_length || payload || padding) is a
     * multiple of block; padding is at least 4 bytes */
    size_t unpadded = 4 + 1 + len;
    size_t pad = block - (unpadded % block);
    if (pad < MIN_PADDING_SIZE) pad += block;

    uint32_t packet_length = (uint32_t)(1 + len + pad);
    size_t total = 4 + (size_t)packet_length;
    if (total > MAX_PACKET_SIZE) return -1;

    size_t mac_len = (d->encrypted && d->mac_key_len > 0) ? SHA256_DIGEST_SIZE : 0;
    uint8_t *buf = malloc(total + mac_len);
    if (!buf) return -1;

    put_be32(buf, packet_length);
    buf[4] = (uint8_t)pad;
    memcpy(buf + 5, payload, len);
    if (rand_bytes(buf + 5 + len, pad) != 0) {
        free(buf);
        return -1;
    }

    if (d->encrypted) {
        if (mac_len > 0) {
            /* mac = HASH(key, seq_be || unencrypted_packet), appended after
             * the ciphertext (RFC 4253 §6.4) — compute before encrypting */
            uint8_t mac[SHA256_DIGEST_SIZE];
            uint8_t seq_be[4];
            put_be32(seq_be, d->seq);
            hmac_sha256_ctx_t h;
            hmac_sha256_init(&h, d->mac_key, d->mac_key_len);
            hmac_sha256_update(&h, seq_be, 4);
            hmac_sha256_update(&h, buf, total);
            hmac_sha256_final(&h, mac);
            memcpy(buf + total, mac, mac_len);
        }
        /* encrypt everything before the MAC (in-place is safe for CTR) */
        aes_ctr_crypt(&d->ctr, buf, buf, total);
        if (net_write_full(s->sock, buf, total + mac_len) !=
            (ssize_t)(total + mac_len)) {
            free(buf);
            return -1;
        }
    } else {
        if (net_write_full(s->sock, buf, total) != (ssize_t)total) {
            free(buf);
            return -1;
        }
    }

    free(buf);
    d->seq++;
    return 0;
}

int session_send_buf(ssh_session_t *s, const ssh_buf_t *payload)
{
    if (!payload) return -1;
    return session_send(s, payload->data, payload->len);
}

int session_recv(ssh_session_t *s, ssh_buf_t *out)
{
    if (!s || !out) return -1;

    ssh_direction_t *d = &s->in;
    uint8_t *pkt = NULL;
    size_t   total = 0;
    uint32_t packet_length = 0;

    if (!d->encrypted) {
        uint8_t head[4];
        if (net_read_full(s->sock, head, 4) != 4) return -1;
        packet_length = get_be32(head);
        /* packet_length >= padding_length(1) + payload(1) + padding(4) */
        if (packet_length < 6 || packet_length > MAX_PACKET_SIZE) return -1;

        total = 4 + (size_t)packet_length;
        pkt = malloc(total);
        if (!pkt) return -1;
        memcpy(pkt, head, 4);
        if (net_read_full(s->sock, pkt + 4, packet_length) !=
            (ssize_t)packet_length) {
            free(pkt);
            return -1;
        }
    } else {
        /* the entire packet including packet_length is encrypted; read one
         * block first to learn the length (RFC 4253 §6.3) */
        uint8_t first[SSH_CIPHER_BLOCK];
        if (net_read_full(s->sock, first, SSH_CIPHER_BLOCK) !=
            (ssize_t)SSH_CIPHER_BLOCK) {
            return -1;
        }
        aes_ctr_crypt(&d->ctr, first, first, SSH_CIPHER_BLOCK);
        packet_length = get_be32(first);
        if (packet_length < 12 || packet_length > MAX_PACKET_SIZE) return -1;
        if ((4 + packet_length) % SSH_CIPHER_BLOCK != 0) return -1;

        total = 4 + (size_t)packet_length;
        pkt = malloc(total);
        if (!pkt) return -1;
        memcpy(pkt, first, SSH_CIPHER_BLOCK);

        size_t rest = total - SSH_CIPHER_BLOCK;
        if (rest > 0) {
            if (net_read_full(s->sock, pkt + SSH_CIPHER_BLOCK, rest) !=
                (ssize_t)rest) {
                free(pkt);
                return -1;
            }
            aes_ctr_crypt(&d->ctr, pkt + SSH_CIPHER_BLOCK,
                          pkt + SSH_CIPHER_BLOCK, rest);
        }

        if (d->mac_key_len > 0) {
            uint8_t got[SHA256_DIGEST_SIZE];
            if (net_read_full(s->sock, got, SHA256_DIGEST_SIZE) !=
                (ssize_t)SHA256_DIGEST_SIZE) {
                free(pkt);
                return -1;
            }
            uint8_t seq_be[4];
            put_be32(seq_be, d->seq);
            hmac_sha256_ctx_t h;
            hmac_sha256_init(&h, d->mac_key, d->mac_key_len);
            hmac_sha256_update(&h, seq_be, 4);
            hmac_sha256_update(&h, pkt, total);
            uint8_t expect[SHA256_DIGEST_SIZE];
            hmac_sha256_final(&h, expect);
            if (!ct_equal(expect, got, SHA256_DIGEST_SIZE)) {
                free(pkt);
                return -1; /* MAC failure — caller should disconnect */
            }
        }
    }

    /* extract payload */
    uint8_t padding_length = pkt[4];
    if (padding_length < MIN_PADDING_SIZE ||
        (size_t)padding_length + 1 >= total) {
        free(pkt);
        return -1;
    }
    size_t payload_len = total - 4 - 1 - padding_length;
    if (payload_len < 1) {
        free(pkt);
        return -1;
    }

    buf_reset(out);
    if (buf_put_raw(out, pkt + 5, payload_len) != 0) {
        free(pkt);
        return -1;
    }
    free(pkt);

    d->seq++;
    return 0;
}
