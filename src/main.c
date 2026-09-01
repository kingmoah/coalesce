#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "net.h"
#include "ssh.h"
#include "buffer.h"
#include "packet.h"

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

    /* 1. Send version identification */
    if (net_write_full(client_sock, IDENT_STRING, sizeof(IDENT_STRING) - 1) < 0) {
        fprintf(stderr, "[server] write ident error: %s\n", net_get_error());
        net_close(client_sock);
        net_close(fd);
        return -1;
    }

    /* 2. Read client version identification */
    char client_ident[256];
    if (net_read_line(client_sock, client_ident, sizeof(client_ident)) <= 0) {
        fprintf(stderr, "[server] read client ident error: %s\n", net_get_error());
        net_close(client_sock);
        net_close(fd);
        return -1;
    }
    printf("[server] Client identification: %s", client_ident);

    /* 3. Receive initial binary packet from client */
    uint32_t seq_in = 0;
    uint32_t seq_out = 0;
    ssh_pkt_t pkt;
    pkt_init(&pkt);

    if (pkt_recv(client_sock, &pkt, &seq_in, 8) == 0) {
        uint8_t msg_type = 0;
        if (buf_get_u8(&pkt.payload, &msg_type) == 0) {
            printf("[server] Received packet msg_type=%d, payload_len=%zu\n",
                   msg_type, buf_len(&pkt.payload));
        }

        /* Echo back an SSH_MSG_DEBUG packet */
        ssh_buf_t reply;
        buf_init(&reply, 64);
        buf_put_u8(&reply, SSH_MSG_DEBUG);
        buf_put_bool(&reply, true);
        buf_put_cstring(&reply, "Coalesce Phase 1 binary packet framed successfully");
        buf_put_cstring(&reply, "");

        pkt_send_buf(client_sock, &reply, &seq_out, 8);
        buf_free(&reply);
    }

    pkt_free(&pkt);
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

    /* 1. Send client identification */
    if (net_write_full(s, IDENT_STRING, sizeof(IDENT_STRING) - 1) < 0) {
        fprintf(stderr, "[client] write ident error: %s\n", net_get_error());
        net_close(s);
        return -1;
    }

    /* 2. Read server identification */
    char server_ident[256];
    if (net_read_line(s, server_ident, sizeof(server_ident)) <= 0) {
        fprintf(stderr, "[client] read server ident error: %s\n", net_get_error());
        net_close(s);
        return -1;
    }
    printf("[client] Server identification: %s", server_ident);

    /* 3. Send test binary packet (SSH_MSG_IGNORE) */
    uint32_t seq_out = 0;
    uint32_t seq_in = 0;
    ssh_buf_t msg;
    buf_init(&msg, 64);
    buf_put_u8(&msg, SSH_MSG_IGNORE);
    buf_put_cstring(&msg, "coalesce test packet");

    if (pkt_send_buf(s, &msg, &seq_out, 8) != 0) {
        fprintf(stderr, "[client] Failed to send packet\n");
    } else {
        printf("[client] Sent SSH_MSG_IGNORE packet (seq=%u)\n", seq_out - 1);
    }
    buf_free(&msg);

    /* 4. Receive reply packet */
    ssh_pkt_t reply_pkt;
    pkt_init(&reply_pkt);
    if (pkt_recv(s, &reply_pkt, &seq_in, 8) == 0) {
        uint8_t msg_type = 0;
        buf_get_u8(&reply_pkt.payload, &msg_type);
        if (msg_type == SSH_MSG_DEBUG) {
            bool display = false;
            buf_get_bool(&reply_pkt.payload, &display);
            char *dbg_msg = buf_get_cstring(&reply_pkt.payload);
            printf("[client] Received SSH_MSG_DEBUG (always_display=%d): %s\n",
                   display, dbg_msg ? dbg_msg : "");
            free(dbg_msg);
        } else {
            printf("[client] Received packet msg_type=%d, payload_len=%zu\n",
                   msg_type, buf_len(&reply_pkt.payload));
        }
    }
    pkt_free(&reply_pkt);

    net_close(s);
    printf("[client] Connection closed.\n");
    return 0;
}   