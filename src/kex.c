#include "kex.h"
#include "ssh.h"
#include "sha256.h"
#include "rand.h"
#include <stdlib.h>
#include <string.h>

#define DEFAULT_KEX "curve25519-sha256"
#define DEFAULT_HOSTKEY "ssh-ed25519"
#define DEFAULT_CIPHERS "aes128-ctr,aes256-ctr"
#define DEFAULT_MACS "hmac-sha2-256"
#define DEFAULT_COMP "none"

static char *strdup_safe(const char *string)
{
    if (string == NULL) return NULL;
    size_t len = strlen(string);
    char *copy = malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, string, len + 1);
    }
    return copy;
}

void kex_init_default(ssh_kex_init_t *kex, bool is_server)
{
    if (kex == NULL) return;
    memset(kex, 0, sizeof(*kex));

    /* Random 16-byte cookie (RFC 4253 §7.1) */
    if (rand_bytes(kex->cookie, 16) != 0) {
        memset(kex->cookie, 0, 16);
    }

    kex->kex_algorithms = strdup_safe(DEFAULT_KEX);
    kex->server_host_key_algorithms = strdup_safe(DEFAULT_HOSTKEY);
    kex->encryption_algorithms_client_to_server = strdup_safe(DEFAULT_CIPHERS);
    kex->encryption_algorithms_server_to_client = strdup_safe(DEFAULT_CIPHERS);
    kex->mac_algorithms_client_to_server = strdup_safe(DEFAULT_MACS);
    kex->mac_algorithms_server_to_client = strdup_safe(DEFAULT_MACS);
    kex->compression_algorithms_client_to_server = strdup_safe(DEFAULT_COMP);
    kex->compression_algorithms_server_to_client = strdup_safe(DEFAULT_COMP);
    kex->languages_client_to_server = strdup_safe("");
    kex->languages_server_to_client = strdup_safe("");
    kex->first_kex_packet_follows = false;
}

int kex_init_encode(const ssh_kex_init_t *kex, ssh_buf_t *buf)
{
    if (!kex || !buf) return -1;

    if (buf_put_u8(buf, SSH_MSG_KEXINIT) != 0) return -1;
    if (buf_put_raw(buf, kex->cookie, 16) != 0) return -1;
    if (buf_put_cstring(buf, kex->kex_algorithms) != 0) return -1;
    if (buf_put_cstring(buf, kex->server_host_key_algorithms) != 0) return -1;
    if (buf_put_cstring(buf, kex->encryption_algorithms_client_to_server) != 0) return -1;
    if (buf_put_cstring(buf, kex->encryption_algorithms_server_to_client) != 0) return -1;
    if (buf_put_cstring(buf, kex->mac_algorithms_client_to_server) != 0) return -1;
    if (buf_put_cstring(buf, kex->mac_algorithms_server_to_client) != 0) return -1;
    if (buf_put_cstring(buf, kex->compression_algorithms_client_to_server) != 0) return -1;
    if (buf_put_cstring(buf, kex->compression_algorithms_server_to_client) != 0) return -1;
    if (buf_put_cstring(buf, kex->languages_client_to_server) != 0) return -1;
    if (buf_put_cstring(buf, kex->languages_server_to_client) != 0) return -1;
    if (buf_put_bool(buf, kex->first_kex_packet_follows) != 0) return -1;
    if (buf_put_u32(buf, 0) != 0) return -1; /* Reserved */

    return 0;
}

int kex_init_decode(ssh_kex_init_t *kex, ssh_buf_t *buf)
{
    if (!kex || !buf) return -1;
    memset(kex, 0, sizeof(*kex));

    uint8_t msg_type = 0;
    if (buf_get_u8(buf, &msg_type) != 0 || msg_type != SSH_MSG_KEXINIT) return -1;
    if (buf_get_raw(buf, kex->cookie, 16) != 0) return -1;

    kex->kex_algorithms = buf_get_cstring(buf);
    kex->server_host_key_algorithms = buf_get_cstring(buf);
    kex->encryption_algorithms_client_to_server = buf_get_cstring(buf);
    kex->encryption_algorithms_server_to_client = buf_get_cstring(buf);
    kex->mac_algorithms_client_to_server = buf_get_cstring(buf);
    kex->mac_algorithms_server_to_client = buf_get_cstring(buf);
    kex->compression_algorithms_client_to_server = buf_get_cstring(buf);
    kex->compression_algorithms_server_to_client = buf_get_cstring(buf);
    kex->languages_client_to_server = buf_get_cstring(buf);
    kex->languages_server_to_client = buf_get_cstring(buf);

    if (buf_get_bool(buf, &kex->first_kex_packet_follows) != 0) return -1;

    uint32_t reserved = 0;
    if (buf_get_u32(buf, &reserved) != 0) return -1;

    return 0;
}

void kex_init_free(ssh_kex_init_t *kex)
{
    if (!kex) return;
    free(kex->kex_algorithms);
    free(kex->server_host_key_algorithms);
    free(kex->encryption_algorithms_client_to_server);
    free(kex->encryption_algorithms_server_to_client);
    free(kex->mac_algorithms_client_to_server);
    free(kex->mac_algorithms_server_to_client);
    free(kex->compression_algorithms_client_to_server);
    free(kex->compression_algorithms_server_to_client);
    free(kex->languages_client_to_server);
    free(kex->languages_server_to_client);
    memset(kex, 0, sizeof(*kex));
}

/* ── Name-List Matching ───────────────────────────────────────── */

static bool list_contains(const char *list, const char *item, size_t item_len)
{
    if (!list || !item || item_len == 0) return false;
    const char *p = list;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        if (len == item_len && strncmp(p, item, len) == 0) {
            return true;
        }
        if (!comma) break;
        p = comma + 1;
    }
    return false;
}

static char *match_list(const char *client_list, const char *server_list)
{
    if (!client_list || !server_list) return NULL;
    const char *p = client_list;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        if (len > 0 && list_contains(server_list, p, len)) {
            char *matched = malloc(len + 1);
            if (matched) {
                memcpy(matched, p, len);
                matched[len] = '\0';
            }
            return matched;
        }
        if (!comma) break;
        p = comma + 1;
    }
    return NULL;
}

int kex_negotiate(const ssh_kex_init_t *c, const ssh_kex_init_t *s, ssh_kex_proposal_t *chosen)
{
    if (!c || !s || !chosen) return -1;
    memset(chosen, 0, sizeof(*chosen));

    chosen->kex        = match_list(c->kex_algorithms, s->kex_algorithms);
    chosen->host_key   = match_list(c->server_host_key_algorithms, s->server_host_key_algorithms);
    chosen->cipher_c2s = match_list(c->encryption_algorithms_client_to_server, s->encryption_algorithms_client_to_server);
    chosen->cipher_s2c = match_list(c->encryption_algorithms_server_to_client, s->encryption_algorithms_server_to_client);
    chosen->mac_c2s    = match_list(c->mac_algorithms_client_to_server, s->mac_algorithms_client_to_server);
    chosen->mac_s2c    = match_list(c->mac_algorithms_server_to_client, s->mac_algorithms_server_to_client);
    chosen->comp_c2s   = match_list(c->compression_algorithms_client_to_server, s->compression_algorithms_client_to_server);
    chosen->comp_s2c   = match_list(c->compression_algorithms_server_to_client, s->compression_algorithms_server_to_client);

    if (!chosen->kex || !chosen->host_key || !chosen->cipher_c2s || !chosen->cipher_s2c ||
        !chosen->mac_c2s || !chosen->mac_s2c || !chosen->comp_c2s || !chosen->comp_s2c) {
        kex_proposal_free(chosen);
        return -1; /* Negotiation failed - no mutually acceptable algorithms */
    }

    return 0;
}

void kex_proposal_free(ssh_kex_proposal_t *p)
{
    if (!p) return;
    free(p->kex);
    free(p->host_key);
    free(p->cipher_c2s);
    free(p->cipher_s2c);
    free(p->mac_c2s);
    free(p->mac_s2c);
    free(p->comp_c2s);
    free(p->comp_s2c);
    memset(p, 0, sizeof(*p));
}

/* ── Key Derivation (RFC 4253 §7.2) ───────────────────────────── */

int kex_derive_key(char letter, const uint8_t *k_mpint, size_t k_len,
                    const uint8_t *h, size_t h_len,
                    const uint8_t *session_id, size_t session_id_len,
                    uint8_t *out_key, size_t key_len)
{
    if (!k_mpint || !h || !session_id || !out_key || key_len == 0) return -1;

    /* K1 = HASH(K || H || X || session_id) */
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, k_mpint, k_len);
    sha256_update(&ctx, h, h_len);
    sha256_update(&ctx, &letter, 1);
    sha256_update(&ctx, session_id, session_id_len);

    uint8_t digest[SHA256_DIGEST_SIZE];
    sha256_final(&ctx, digest);

    size_t written = 0;
    size_t chunk = key_len < SHA256_DIGEST_SIZE ? key_len : SHA256_DIGEST_SIZE;
    memcpy(out_key, digest, chunk);
    written += chunk;

    /* If key_len > SHA256_DIGEST_SIZE, compute K2 = HASH(K || H || K1), etc. */
    while (written < key_len) {
        sha256_init(&ctx);
        sha256_update(&ctx, k_mpint, k_len);
        sha256_update(&ctx, h, h_len);
        sha256_update(&ctx, out_key, written);
        sha256_final(&ctx, digest);

        chunk = (key_len - written) < SHA256_DIGEST_SIZE ? (key_len - written) : SHA256_DIGEST_SIZE;
        memcpy(out_key + written, digest, chunk);
        written += chunk;
    }

    return 0;
}
