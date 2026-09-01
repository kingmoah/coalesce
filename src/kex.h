#ifndef COALESCE_KEX_H
#define COALESCE_KEX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "buffer.h"

typedef struct {
    uint8_t cookie[16];
    char   *kex_algorithms;
    char   *server_host_key_algorithms;
    char   *encryption_algorithms_client_to_server;
    char   *encryption_algorithms_server_to_client;
    char   *mac_algorithms_client_to_server;
    char   *mac_algorithms_server_to_client;
    char   *compression_algorithms_client_to_server;
    char   *compression_algorithms_server_to_client;
    char   *languages_client_to_server;
    char   *languages_server_to_client;
    bool    first_kex_packet_follows;
} ssh_kex_init_t;

typedef struct {
    char *kex;
    char *host_key;
    char *cipher_c2s;
    char *cipher_s2c;
    char *mac_c2s;
    char *mac_s2c;
    char *comp_c2s;
    char *comp_s2c;
} ssh_kex_proposal_t;

/* KEXINIT lifecycle */
void kex_init_default(ssh_kex_init_t *kex, bool is_server);
int  kex_init_encode(const ssh_kex_init_t *kex, ssh_buf_t *buf);
int  kex_init_decode(ssh_kex_init_t *kex, ssh_buf_t *buf);
void kex_init_free(ssh_kex_init_t *kex);

/* Proposal match algorithm */
int  kex_negotiate(const ssh_kex_init_t *client_kex, const ssh_kex_init_t *server_kex, ssh_kex_proposal_t *chosen);
void kex_proposal_free(ssh_kex_proposal_t *proposal);

/* Key Derivation Function (RFC 4253 §7.2) */
int  kex_derive_key(char letter, const uint8_t *k_mpint, size_t k_len,
                    const uint8_t *h, size_t h_len,
                    const uint8_t *session_id, size_t session_id_len,
                    uint8_t *out_key, size_t key_len);

#endif /* COALESCE_KEX_H */
