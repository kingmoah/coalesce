/* net.c */
#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #pragma comment(lib, "ws2_32")
  static WSADATA g_wsa;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
#endif





int net_init(void)
{
#ifdef _WIN32
    return WSAStartup(MAKEWORD(2, 2), &g_wsa) == 0 ? 0 : -1;
#else
    return 0;
#endif
}

void net_shutdown(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

net_socket_t net_listen(const char *host, int port)
{
    net_socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (!NET_IS_VALID(s)) return NET_INVALID;

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof opt);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons((uint16_t)port),
        .sin_addr   = { .s_addr = host ? inet_addr(host) : INADDR_ANY }
    };

    if (bind(s, (struct sockaddr *)&addr, sizeof addr) < 0 ||
        listen(s, 16) < 0) {
        net_close(s);
        return NET_INVALID;
    }
    return s;
}

net_socket_t net_accept(net_socket_t srv, struct net_addr *out_addr)
{
    struct sockaddr_in ca;
    socklen_t cl = sizeof ca;
    net_socket_t c = accept(srv, (struct sockaddr *)&ca, &cl);
    if (!NET_IS_VALID(c)) return NET_INVALID;

    if (out_addr) {
        inet_ntop(AF_INET, &ca.sin_addr, out_addr->ip, sizeof out_addr->ip);
        out_addr->port = ntohs(ca.sin_port);
    }
    return c;
}

net_socket_t net_connect(const char *host, int port)
{
    net_socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (!NET_IS_VALID(s)) return NET_INVALID;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons((uint16_t)port),
    };

    /* resolve hostname OR numeric IP */
    struct hostent *he = gethostbyname(host);
    if (he && he->h_addr_list[0])
        memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof addr.sin_addr);
    else
        addr.sin_addr.s_addr = inet_addr(host);  /* fallback for numeric */

    if (connect(s, (struct sockaddr *)&addr, sizeof addr) < 0) {
        net_close(s);
        return NET_INVALID;
    }

    return s;
}   

int net_set_nonblocking(net_socket_t s)
{
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    return fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

int net_shutdown_write(net_socket_t s)
{
    return shutdown(s, NET_SHUT_WRITE);
}

void net_close(net_socket_t s)
{
    if (NET_IS_VALID(s)) {
#ifdef _WIN32
        closesocket(s);
#else
        close(s);
#endif
    }
}

ssize_t net_read_full(net_socket_t s, void *buf, size_t n)
{
    size_t got = 0;
    while (got < n) {
        ssize_t r = recv(s, (char *)buf + got, n - got, 0);
        if (r <= 0) return r;
        got += (size_t)r;
    }
    return (ssize_t)got;
}

ssize_t net_write_full(net_socket_t s, const void *buf, size_t n)
{
    size_t sent = 0;
    while (sent < n) {
        ssize_t w = send(s, (const char *)buf + sent, n - sent, 0);
        if (w <= 0) return w;
        sent += (size_t)w;
    }
    return (ssize_t)sent;
}

ssize_t net_read_line(net_socket_t s, char *buf, size_t max)
{
    size_t n = 0;
    while (n + 1 < max) {
        ssize_t r = recv(s, buf + n, 1, 0);
        if (r <= 0) return r;
        n++;
        if (buf[n - 1] == '\n') break;
    }
    buf[n] = '\0';
    return (ssize_t)n;
}   

const char *net_get_error(void)
{
#ifdef _WIN32
    int err = WSAGetLastError();
    static char buf[128];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, 0, buf, sizeof buf, NULL);
    /* strip trailing \r\n */
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
        buf[--len] = '\0';
    return buf;
#else
    return strerror(errno);
#endif
}   