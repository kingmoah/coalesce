#include "packet.h"
#include "ssh.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
  #include <windows.h>
  #include <wincrypt.h>
#else
  #include <fcntl.h>
  #include <unistd.h>
#endif

static int get_random_bytes(uint8_t *dst, size_t len)
{
#ifdef _WIN32
    HCRYPTPROV prov = 0;
    if (CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        BOOL ok = CryptGenRandom(prov, (DWORD)len, dst);
        CryptReleaseContext(prov, 0);
        if (ok) return 0;
    }
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t r = read(fd, dst, len);
        close(fd);
        if (r == (ssize_t)len) return 0;
    }
#endif
    /* Fallback PRNG */
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }
    for (size_t i = 0; i < len; i++) {
        dst[i] = (uint8_t)(rand() & 0xFF);
    }
    return 0;
}

void pkt_init(ssh_pkt_t *pkt)
{
    if (!pkt) return;
    pkt->packet_length = 0;
    pkt->padding_length = 0;
    buf_init(&pkt->payload, 256);
    pkt->padding = NULL;
    pkt->mac = NULL;
    pkt->mac_len = 0;
}

void pkt_free(ssh_pkt_t *pkt)
{
    if (!pkt) return;
    buf_free(&pkt->payload);
    if (pkt->padding) {
        free(pkt->padding);
        pkt->padding = NULL;
    }
    if (pkt->mac) {
        free(pkt->mac);
        pkt->mac = NULL;
    }
    pkt->packet_length = 0;
    pkt->padding_length = 0;
    pkt->mac_len = 0;
}

void pkt_reset(ssh_pkt_t *pkt)
{
    if (!pkt) return;
    buf_reset(&pkt->payload);
    if (pkt->padding) {
        free(pkt->padding);
        pkt->padding = NULL;
    }
    if (pkt->mac) {
        free(pkt->mac);
        pkt->mac = NULL;
    }
    pkt->packet_length = 0;
    pkt->padding_length = 0;
    pkt->mac_len = 0;
}

int pkt_send(net_socket_t s, const uint8_t *payload, size_t payload_len, uint32_t *seq, size_t block_size)
{
    if (!payload && payload_len > 0) return -1;
    if (block_size < 8) block_size = 8;

    /*
     * RFC 4253 §6:
     * total_len = 4 (packet_length) + 1 (padding_length) + payload_len + padding_len
     * total_len must be a multiple of block_size (or 8).
     * padding_len must be between 4 and 255.
     */
    size_t unpadded_len = 4 + 1 + payload_len;
    size_t pad_len = block_size - (unpadded_len % block_size);
    if (pad_len < MIN_PADDING_SIZE) {
        pad_len += block_size;
    }

    uint32_t packet_len = (uint32_t)(1 + payload_len + pad_len);
    size_t total_wire_len = 4 + packet_len;

    uint8_t *wire_buf = malloc(total_wire_len);
    if (!wire_buf) return -1;

    /* packet_length (big-endian) */
    wire_buf[0] = (uint8_t)((packet_len >> 24) & 0xFF);
    wire_buf[1] = (uint8_t)((packet_len >> 16) & 0xFF);
    wire_buf[2] = (uint8_t)((packet_len >> 8) & 0xFF);
    wire_buf[3] = (uint8_t)(packet_len & 0xFF);

    /* padding_length */
    wire_buf[4] = (uint8_t)pad_len;

    /* payload */
    if (payload_len > 0) {
        memcpy(wire_buf + 5, payload, payload_len);
    }

    /* random padding */
    get_random_bytes(wire_buf + 5 + payload_len, pad_len);

    /* Send wire buffer */
    ssize_t sent = net_write_full(s, wire_buf, total_wire_len);
    free(wire_buf);

    if (sent < 0 || (size_t)sent != total_wire_len) {
        return -1;
    }

    if (seq) (*seq)++;
    return 0;
}

int pkt_send_buf(net_socket_t s, const ssh_buf_t *payload, uint32_t *seq, size_t block_size)
{
    if (!payload) return -1;
    return pkt_send(s, payload->data, payload->len, seq, block_size);
}

int pkt_recv(net_socket_t s, ssh_pkt_t *pkt, uint32_t *seq, size_t block_size)
{
    if (!pkt) return -1;
    pkt_reset(pkt);

    if (block_size < 8) block_size = 8;

    /* Read packet_length (4 bytes) */
    uint8_t len_buf[4];
    ssize_t r = net_read_full(s, len_buf, 4);
    if (r <= 0 || r != 4) return -1;

    uint32_t packet_length = ((uint32_t)len_buf[0] << 24) |
                             ((uint32_t)len_buf[1] << 16) |
                             ((uint32_t)len_buf[2] << 8) |
                             ((uint32_t)len_buf[3]);

    if (packet_length < 5 || packet_length > MAX_PACKET_SIZE) {
        return -1; /* Corrupted or out of bounds */
    }

    /* Read the rest of the packet (packet_length bytes) */
    uint8_t *body = malloc(packet_length);
    if (!body) return -1;

    r = net_read_full(s, body, packet_length);
    if (r <= 0 || (size_t)r != packet_length) {
        free(body);
        return -1;
    }

    uint8_t padding_length = body[0];
    if (padding_length < MIN_PADDING_SIZE || padding_length >= packet_length) {
        free(body);
        return -1; /* Invalid padding */
    }

    size_t payload_len = packet_length - 1 - padding_length;

    pkt->packet_length = packet_length;
    pkt->padding_length = padding_length;

    /* Copy payload into pkt->payload */
    if (payload_len > 0) {
        if (buf_put_raw(&pkt->payload, body + 1, payload_len) != 0) {
            free(body);
            return -1;
        }
    }

    free(body);
    if (seq) (*seq)++;
    return 0;
}
