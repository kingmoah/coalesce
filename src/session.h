/* session.h — SSH transport session: version banners, key state and the
 * binary packet protocol (RFC 4253 §4.2, §6, §7.3). */
#ifndef COALESCE_SESSION_H
#define COALESCE_SESSION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "net.h"
#include "buffer.h"
#include "aes.h"

/* RFC 4253 §4.2: max identification line length, including CR LF. */
#define SSH_IDENT_MAX 255

/* hmac-sha2-256 uses a 32-byte key (RFC 6668 §2). */
#define SSH_MAC_KEY_MAX 32

typedef enum {
    SSH_ROLE_CLIENT = 0,
    SSH_ROLE_SERVER = 1
} ssh_role_t;

/* One direction of the transport. The encrypted byte stream (and its MAC
 * sequence numbers) is continuous per direction; sequence numbers start at
 * zero and are never reset, even across rekeys (RFC 4253 §6.4). */
typedef struct {
    bool          encrypted;                    /* cipher + MAC active */
    aes_ctr_ctx_t ctr;                          /* AES-CTR keystream state */
    uint8_t       mac_key[SSH_MAC_KEY_MAX];
    size_t        mac_key_len;                  /* 0 = no MAC */
    uint32_t      seq;
} ssh_direction_t;

typedef struct ssh_session {
    net_socket_t sock;

    /* identification strings, CR/LF excluded (RFC 4253 §8) */
    char   v_c[SSH_IDENT_MAX + 1];
    size_t v_c_len;
    char   v_s[SSH_IDENT_MAX + 1];
    size_t v_s_len;

    /* raw KEXINIT payloads incl. message-type byte (RFC 4253 §8) */
    uint8_t *i_c;
    size_t   i_c_len;
    uint8_t *i_s;
    size_t   i_s_len;

    /* first exchange hash, constant across rekeys (RFC 4253 §7.2) */
    uint8_t  session_id[32];
    bool     session_id_set;

    ssh_direction_t out;  /* ours  (c2s on client, s2c on server) */
    ssh_direction_t in;   /* theirs (s2c on client, c2s on server) */
} ssh_session_t;

void session_init(ssh_session_t *s, net_socket_t sock);
void session_free(ssh_session_t *s);

/* Validate an identification line (RFC 4253 §4.2): "SSH-2.0-..." or
 * "SSH-1.99-...", printable US-ASCII, optional " SP comments", at most
 * 255 bytes including CR LF. Returns 0 if usable. */
int  session_ident_check(const char *line, size_t len);

/* Version banner exchange with hardening: sends our identification string,
 * skips non-"SSH-" pre-lines the peer may send, enforces the line limit and
 * rejects "SSH-1.x" (except 1.99). Stores both V_C and V_S, CR/LF excluded. */
int  session_exchange_ident(ssh_session_t *s, ssh_role_t role);

/* Install key material for one direction. The direction stays in plaintext
 * mode until session_activate() is called (after SSH_MSG_NEWKEYS). */
int  session_set_keys(ssh_direction_t *dir, const char *cipher_name,
                      const uint8_t *iv, size_t iv_len,
                      const uint8_t *key, size_t key_len,
                      const char *mac_name,
                      const uint8_t *mac_key, size_t mac_key_len);

/* Put the direction into encrypted mode. */
void session_activate(ssh_direction_t *dir);

/* Binary packet I/O (RFC 4253 §6). The payload includes the message-type
 * byte; encryption and MAC are applied automatically per direction state. */
int  session_send(ssh_session_t *s, const uint8_t *payload, size_t len);
int  session_send_buf(ssh_session_t *s, const ssh_buf_t *payload);
int  session_recv(ssh_session_t *s, ssh_buf_t *out_payload);

#endif /* COALESCE_SESSION_H */
