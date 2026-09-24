/* base64.c — RFC 4648 base64 encode/decode. */
#include "base64.h"

static const char B64_ALPHA[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64_encode(char *dst, size_t dst_cap, const uint8_t *src, size_t src_len)
{
    if (!dst || !src) return -1;

    size_t need = ((src_len + 2) / 3) * 4;
    if (need + 1 > dst_cap) return -1;

    size_t o = 0;
    size_t i = 0;
    while (i + 3 <= src_len) {
        uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1] << 8) | src[i + 2];
        dst[o++] = B64_ALPHA[(v >> 18) & 0x3F];
        dst[o++] = B64_ALPHA[(v >> 12) & 0x3F];
        dst[o++] = B64_ALPHA[(v >> 6) & 0x3F];
        dst[o++] = B64_ALPHA[v & 0x3F];
        i += 3;
    }
    if (src_len - i == 1) {
        uint32_t v = (uint32_t)src[i] << 16;
        dst[o++] = B64_ALPHA[(v >> 18) & 0x3F];
        dst[o++] = B64_ALPHA[(v >> 12) & 0x3F];
        dst[o++] = '=';
        dst[o++] = '=';
    } else if (src_len - i == 2) {
        uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1] << 8);
        dst[o++] = B64_ALPHA[(v >> 18) & 0x3F];
        dst[o++] = B64_ALPHA[(v >> 12) & 0x3F];
        dst[o++] = B64_ALPHA[(v >> 6) & 0x3F];
        dst[o++] = '=';
    }
    dst[o] = '\0';
    return (int)o;
}

static int b64_val(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

int base64_decode(uint8_t *dst, size_t dst_cap, const char *src)
{
    if (!dst || !src) return -1;

    size_t o = 0;
    uint32_t acc = 0;
    int bits = 0;
    int pad = 0;

    for (const char *p = src; *p; p++) {
        char c = *p;
        if (c == '\n' || c == '\r') continue;
        if (c == '=') { pad++; continue; }
        int v = b64_val(c);
        if (v < 0) return -1;
        if (pad) return -1; /* data after padding */

        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (o >= dst_cap) return -1;
            dst[o++] = (uint8_t)((acc >> bits) & 0xFF);
        }
    }
    if (bits >= 6) return -1; /* dangling quantum */
    if (pad > 2) return -1;
    return (int)o;
}
