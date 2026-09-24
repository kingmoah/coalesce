#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "net.h"
#include "ssh.h"
#include "buffer.h"
#include "session.h"

int server(int port);
int client(const char *user, const char *host, int port);

/* Helper to parse target: user@host[:port] */
static int parse_target(const char *target, char *out_user, size_t user_sz,
                        char *out_host, size_t host_sz, int *out_port)
{
    const char *at = strchr(target, '@');
    if (!at) return -1;

    size_t ulen = (size_t)(at - target);
    if (ulen == 0 || ulen >= user_sz) return -1;
    memcpy(out_user, target, ulen);
    out_user[ulen] = '\0';

    const char *hp = at + 1;
    const char *colon = strchr(hp, ':');
    if (colon) {
        size_t hlen = (size_t)(colon - hp);
        if (hlen == 0 || hlen >= host_sz) return -1;
        memcpy(out_host, hp, hlen);
        out_host[hlen] = '\0';
        *out_port = atoi(colon + 1);
        if (*out_port <= 0 || *out_port > 65535) return -1;
    } else {
        size_t hlen = strlen(hp);
        if (hlen == 0 || hlen >= host_sz) return -1;
        memcpy(out_host, hp, hlen);
        out_host[hlen] = '\0';
        *out_port = 22;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: coalesce serve [port]\n"
                        "       coalesce connect user@host[:port]\n");
        return 1;
    }

    if (net_init() != 0) {
        fprintf(stderr, "net_init failed: %s\n", net_get_error());
        return 1;
    }

    int rc = 0;
    if (strcmp(argv[1], "serve") == 0) {
        int port = 22;
        if (argc > 2)
            port = atoi(argv[2]);
        rc = server(port);
    } else if (strcmp(argv[1], "connect") == 0) {
        if (argc < 3) {
            fprintf(stderr, "usage: coalesce connect user@host[:port]\n");
            net_shutdown();
            return 1;
        }

        char user[128] = {0};
        char host[256] = {0};
        int port = 22;

        if (parse_target(argv[2], user, sizeof(user), host, sizeof(host), &port) != 0) {
            fprintf(stderr, "error: invalid target format '%s' (expected user@host[:port])\n", argv[2]);
            net_shutdown();
            return 1;
        }

        rc = client(user, host, port);
    } else {
        fprintf(stderr, "unknown command: %s\n", argv[1]);
        fprintf(stderr, "usage: coalesce serve [port]\n"
                        "       coalesce connect user@host[:port]\n");
        rc = 1;
    }

    net_shutdown();
    return rc;
}

int server(int port)
{
    printf("[server] Listening on 0.0.0.0:%d ...\n", port);
    net_socket_t fd = net_listen("0.0.0.0", port);
    if (!NET_IS_VALID(fd)) {
        fprintf(stderr, "[server] net_listen failed: %s\n", net_get_error());
        return -1;
    }

    struct net_addr client_addr = {0};
    net_socket_t client_sock = net_accept(fd, &client_addr);
    if (!NET_IS_VALID(client_sock)) {
        fprintf(stderr, "[server] net_accept failed: %s\n", net_get_error());
        net_close(fd);
        return -1;
    }
    printf("[server] Connection accepted from %s:%d\n", client_addr.ip, client_addr.port);

    ssh_session_t sess;
    session_init(&sess, client_sock);

    /* 1. Hardened version banner exchange (RFC 4253 §4.2) */
    if (session_exchange_ident(&sess, SSH_ROLE_SERVER) != 0) {
        fprintf(stderr, "[server] identification exchange failed\n");
        session_free(&sess);
        net_close(client_sock);
        net_close(fd);
        return -1;
    }
    printf("[server] Client identification: %s\n", sess.v_c);

    /* 2. Receive the first binary packet (plaintext phase, RFC 4253 §6) */
    ssh_buf_t pkt;
    buf_init(&pkt, 256);

    if (session_recv(&sess, &pkt) == 0) {
        uint8_t msg_type = 0;
        if (buf_get_u8(&pkt, &msg_type) == 0) {
            printf("[server] Received packet msg_type=%d, payload_len=%zu, seq=%u\n",
                   msg_type, buf_len(&pkt), sess.in.seq - 1);
        }

        /* Echo back an SSH_MSG_DEBUG packet */
        ssh_buf_t reply;
        buf_init(&reply, 64);
        buf_put_u8(&reply, SSH_MSG_DEBUG);
        buf_put_bool(&reply, true);
        buf_put_cstring(&reply, "Coalesce session layer framed the packet successfully");
        buf_put_cstring(&reply, "");
        session_send_buf(&sess, &reply);
        buf_free(&reply);
    } else {
        fprintf(stderr, "[server] packet receive failed\n");
    }

    buf_free(&pkt);
    session_free(&sess);
    net_close(client_sock);
    net_close(fd);
    printf("[server] Session ended cleanly.\n");
    return 0;
}

int client(const char *user, const char *host, int port)
{
    printf("[client] Connecting to %s@%s:%d ...\n", user, host, port);
    net_socket_t s = net_connect(host, port);
    if (!NET_IS_VALID(s)) {
        fprintf(stderr, "[client] net_connect failed: %s\n", net_get_error());
        return -1;
    }

    ssh_session_t sess;
    session_init(&sess, s);

    /* 1. Hardened version banner exchange (RFC 4253 §4.2) */
    if (session_exchange_ident(&sess, SSH_ROLE_CLIENT) != 0) {
        fprintf(stderr, "[client] identification exchange failed\n");
        session_free(&sess);
        net_close(s);
        return -1;
    }
    printf("[client] Server identification: %s\n", sess.v_s);

    /* 2. Send test binary packet (SSH_MSG_IGNORE) */
    ssh_buf_t msg;
    buf_init(&msg, 64);
    buf_put_u8(&msg, SSH_MSG_IGNORE);
    buf_put_cstring(&msg, "coalesce test packet");

    if (session_send_buf(&sess, &msg) != 0) {
        fprintf(stderr, "[client] Failed to send packet\n");
    } else {
        printf("[client] Sent SSH_MSG_IGNORE packet (seq=%u)\n", sess.out.seq - 1);
    }
    buf_free(&msg);

    /* 3. Receive reply packet */
    ssh_buf_t reply;
    buf_init(&reply, 256);
    if (session_recv(&sess, &reply) == 0) {
        uint8_t msg_type = 0;
        buf_get_u8(&reply, &msg_type);
        if (msg_type == SSH_MSG_DEBUG) {
            bool display = false;
            buf_get_bool(&reply, &display);
            char *dbg_msg = buf_get_cstring(&reply);
            printf("[client] Received SSH_MSG_DEBUG (always_display=%d): %s\n",
                   display, dbg_msg ? dbg_msg : "");
            free(dbg_msg);
        } else {
            printf("[client] Received packet msg_type=%d\n", msg_type);
        }
    }
    buf_free(&reply);

    session_free(&sess);
    net_close(s);
    printf("[client] Connection closed.\n");
    return 0;
}
