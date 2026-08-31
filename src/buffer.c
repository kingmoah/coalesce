#include "buffer.h"
#include <stdlib.h>
#include <string.h>

#define BUF_MIN_CAP 64

ssh_buf_t *buf_new(size_t initial_cap)
{
    ssh_buf_t *buf = malloc(sizeof(*buf));
    if (!buf) return NULL;
    if (buf_init(buf, initial_cap) != 0) {
        free(buf);
        return NULL;
    }
    return buf;
}

int buf_init(ssh_buf_t *buf, size_t initial_cap)
{
    if (!buf) return -1;
    if (initial_cap < BUF_MIN_CAP) initial_cap = BUF_MIN_CAP;
    buf->data = malloc(initial_cap);
    if (!buf->data) return -1;
    buf->cap = initial_cap;
    buf->len = 0;
    buf->rpos = 0;
    return 0;
}

void buf_free(ssh_buf_t *buf)
{
    if (!buf) return;
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->cap = 0;
    buf->len = 0;
    buf->rpos = 0;
}

void buf_reset(ssh_buf_t *buf)
{
    if (!buf) return;
    buf->len = 0;
    buf->rpos = 0;
}

int buf_reserve(ssh_buf_t *buf, size_t additional)
{
    if (!buf) return -1;
    if (buf->len + additional <= buf->cap) return 0;

    size_t new_cap = buf->cap ? buf->cap * 2 : BUF_MIN_CAP;
    while (new_cap < buf->len + additional) {
        new_cap *= 2;
    }

    uint8_t *new_data = realloc(buf->data, new_cap);
    if (!new_data) return -1;

    buf->data = new_data;
    buf->cap = new_cap;
    return 0;
}

size_t buf_readable(const ssh_buf_t *buf)
{
    if (!buf || buf->rpos >= buf->len) return 0;
    return buf->len - buf->rpos;
}

const uint8_t *buf_read_ptr(const ssh_buf_t *buf)
{
    if (!buf || !buf->data) return NULL;
    return buf->data + buf->rpos;
}

uint8_t *buf_data(ssh_buf_t *buf)
{
    return buf ? buf->data : NULL;
}

size_t buf_len(const ssh_buf_t *buf)
{
    return buf ? buf->len : 0;
}

int buf_consume(ssh_buf_t *buf, size_t n)
{
    if (!buf || buf->rpos + n > buf->len) return -1;
    buf->rpos += n;
    return 0;
}

/* ── Writers ─────────────────────────────────────────────────── */

int buf_put_u8(ssh_buf_t *buf, uint8_t val)
{
    if (buf_reserve(buf, 1) != 0) return -1;
    buf->data[buf->len++] = val;
    return 0;
}

int buf_put_u32(ssh_buf_t *buf, uint32_t val)
{
    if (buf_reserve(buf, 4) != 0) return -1;
    buf->data[buf->len++] = (uint8_t)((val >> 24) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((val >> 16) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((val >> 8) & 0xFF);
    buf->data[buf->len++] = (uint8_t)(val & 0xFF);
    return 0;
}

int buf_put_u64(ssh_buf_t *buf, uint64_t val)
{
    if (buf_reserve(buf, 8) != 0) return -1;
    for (int i = 7; i >= 0; i--) {
        buf->data[buf->len++] = (uint8_t)((val >> (i * 8)) & 0xFF);
    }
    return 0;
}

int buf_put_bool(ssh_buf_t *buf, bool val)
{
    return buf_put_u8(buf, val ? 1 : 0);
}

int buf_put_raw(ssh_buf_t *buf, const void *src, size_t n)
{
    if (!src && n > 0) return -1;
    if (n == 0) return 0;
    if (buf_reserve(buf, n) != 0) return -1;
    memcpy(buf->data + buf->len, src, n);
    buf->len += n;
    return 0;
}

int buf_put_string(ssh_buf_t *buf, const void *str, size_t len)
{
    if (buf_put_u32(buf, (uint32_t)len) != 0) return -1;
    if (len > 0) {
        if (buf_put_raw(buf, str, len) != 0) return -1;
    }
    return 0;
}

int buf_put_cstring(ssh_buf_t *buf, const char *str)
{
    if (!str) return buf_put_string(buf, "", 0);
    return buf_put_string(buf, str, strlen(str));
}

int buf_put_mpint(ssh_buf_t *buf, const uint8_t *bignum, size_t len)
{
    if (!bignum || len == 0) {
        return buf_put_u32(buf, 0);
    }

    /* Skip leading zero bytes */
    size_t offset = 0;
    while (offset < len && bignum[offset] == 0) {
        offset++;
    }

    if (offset == len) {
        /* Value is zero */
        return buf_put_u32(buf, 0);
    }

    size_t actual_len = len - offset;
    bool need_pad = (bignum[offset] & 0x80) != 0;
    uint32_t wire_len = (uint32_t)(actual_len + (need_pad ? 1 : 0));

    if (buf_put_u32(buf, wire_len) != 0) return -1;
    if (need_pad) {
        if (buf_put_u8(buf, 0x00) != 0) return -1;
    }
    return buf_put_raw(buf, bignum + offset, actual_len);
}

/* ── Readers ─────────────────────────────────────────────────── */

int buf_get_u8(ssh_buf_t *buf, uint8_t *val)
{
    if (!buf || buf_readable(buf) < 1) return -1;
    if (val) *val = buf->data[buf->rpos];
    buf->rpos += 1;
    return 0;
}

int buf_get_u32(ssh_buf_t *buf, uint32_t *val)
{
    if (!buf || buf_readable(buf) < 4) return -1;
    if (val) {
        *val = ((uint32_t)buf->data[buf->rpos] << 24) |
               ((uint32_t)buf->data[buf->rpos + 1] << 16) |
               ((uint32_t)buf->data[buf->rpos + 2] << 8) |
               ((uint32_t)buf->data[buf->rpos + 3]);
    }
    buf->rpos += 4;
    return 0;
}

int buf_get_u64(ssh_buf_t *buf, uint64_t *val)
{
    if (!buf || buf_readable(buf) < 8) return -1;
    if (val) {
        uint64_t res = 0;
        for (int i = 0; i < 8; i++) {
            res = (res << 8) | (uint64_t)buf->data[buf->rpos + i];
        }
        *val = res;
    }
    buf->rpos += 8;
    return 0;
}

int buf_get_bool(ssh_buf_t *buf, bool *val)
{
    uint8_t b = 0;
    if (buf_get_u8(buf, &b) != 0) return -1;
    if (val) *val = (b != 0);
    return 0;
}

int buf_get_raw(ssh_buf_t *buf, void *dst, size_t n)
{
    if (!buf || buf_readable(buf) < n) return -1;
    if (dst && n > 0) {
        memcpy(dst, buf->data + buf->rpos, n);
    }
    buf->rpos += n;
    return 0;
}

int buf_get_string(ssh_buf_t *buf, uint8_t **out_data, size_t *out_len)
{
    if (!buf) return -1;
    uint32_t len = 0;
    if (buf_get_u32(buf, &len) != 0) return -1;
    if (buf_readable(buf) < len) return -1;

    uint8_t *str = malloc((size_t)len + 1);
    if (!str) return -1;

    if (len > 0) {
        memcpy(str, buf->data + buf->rpos, len);
        buf->rpos += len;
    }
    str[len] = '\0';

    if (out_data) *out_data = str;
    else free(str);

    if (out_len) *out_len = (size_t)len;
    return 0;
}

char *buf_get_cstring(ssh_buf_t *buf)
{
    uint8_t *data = NULL;
    size_t len = 0;
    if (buf_get_string(buf, &data, &len) != 0) return NULL;
    return (char *)data;
}

int buf_get_mpint(ssh_buf_t *buf, uint8_t **out_data, size_t *out_len)
{
    return buf_get_string(buf, out_data, out_len);
}
