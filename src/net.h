/* net.h — raw TCP transport utilities. Zero protocol knowledge. */
#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET net_socket_t;
  #define NET_INVALID INVALID_SOCKET
  #define NET_SHUT_WRITE SD_SEND
  #define NET_IS_VALID(s) ((s) != INVALID_SOCKET)
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  typedef int net_socket_t;
  #define NET_INVALID (-1)
  #define NET_SHUT_WRITE SHUT_WR
  #define NET_IS_VALID(s) ((s) >= 0)
#endif

/* peer address (for accept / connect) */
struct net_addr {
    char ip[INET_ADDRSTRLEN];
    uint16_t port;
};

/* lifecycle */
int  net_init(void);              /* 0 on success */
void net_shutdown(void);

/* error */
const char *net_get_error(void);   

/* server */
net_socket_t net_listen(const char *host, int port);
net_socket_t net_accept(net_socket_t srv, struct net_addr *out_addr);

/* client */
net_socket_t net_connect(const char *host, int port);

/* I/O */
int  net_set_nonblocking(net_socket_t s);
int  net_shutdown_write(net_socket_t s);
void net_close(net_socket_t s);

/* read/write exactly n bytes (blocking) */
ssize_t net_read_full(net_socket_t s, void *buf, size_t n);
ssize_t net_write_full(net_socket_t s, const void *buf, size_t n);

/* read one line (up to and including '\n'), max max-1 bytes + NUL */
ssize_t net_read_line(net_socket_t s, char *buf, size_t max);

#endif /* NET_H */   