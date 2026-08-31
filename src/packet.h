#ifndef COALESCE_PACKET_H
#define COALESCE_PACKET_H

#include <stdint.h>
#include <stddef.h>
#include "buffer.h"
#include "net.h"

typedef struct ssh_pkt {
    uint32_t   packet_length;
    uint8_t    padding_length;
    ssh_buf_t  payload;
    uint8_t   *padding;
    uint8_t   *mac;
    size_t     mac_len;
} ssh_pkt_t;

/* Packet lifecycle */
void pkt_init(ssh_pkt_t *pkt);
void pkt_free(ssh_pkt_t *pkt);
void pkt_reset(ssh_pkt_t *pkt);

/* High-level packet send & receive */
int  pkt_send(net_socket_t s, const uint8_t *payload, size_t payload_len, uint32_t *seq, size_t block_size);
int  pkt_send_buf(net_socket_t s, const ssh_buf_t *payload, uint32_t *seq, size_t block_size);
int  pkt_recv(net_socket_t s, ssh_pkt_t *pkt, uint32_t *seq, size_t block_size);

#endif /* COALESCE_PACKET_H */