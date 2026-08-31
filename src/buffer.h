#ifndef COALESCE_BUFFER_H
#define COALESCE_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct ssh_buf {
    uint8_t *data;
    size_t   rpos;    /* Read position */
    size_t   len;     /* Current written length */
    size_t   cap;     /* Total allocated capacity */
} ssh_buf_t;

/* Buffer lifecycle */
ssh_buf_t *buf_new(size_t initial_cap);
int        buf_init(ssh_buf_t *buf, size_t initial_cap);
void       buf_free(ssh_buf_t *buf);
void       buf_reset(ssh_buf_t *buf);
int        buf_reserve(ssh_buf_t *buf, size_t additional);

/* Status and pointer access */
size_t     buf_readable(const ssh_buf_t *buf);
const uint8_t *buf_read_ptr(const ssh_buf_t *buf);
uint8_t   *buf_data(ssh_buf_t *buf);
size_t     buf_len(const ssh_buf_t *buf);
int        buf_consume(ssh_buf_t *buf, size_t n);

/* Writers (Appenders - return 0 on success, -1 on allocation failure) */
int buf_put_u8(ssh_buf_t *buf, uint8_t val);
int buf_put_u32(ssh_buf_t *buf, uint32_t val);
int buf_put_u64(ssh_buf_t *buf, uint64_t val);
int buf_put_bool(ssh_buf_t *buf, bool val);
int buf_put_raw(ssh_buf_t *buf, const void *src, size_t n);
int buf_put_string(ssh_buf_t *buf, const void *str, size_t len);
int buf_put_cstring(ssh_buf_t *buf, const char *str);
int buf_put_mpint(ssh_buf_t *buf, const uint8_t *bignum, size_t len);

/* Readers (Extractors - return 0 on success, -1 on buffer underflow) */
int buf_get_u8(ssh_buf_t *buf, uint8_t *val);
int buf_get_u32(ssh_buf_t *buf, uint32_t *val);
int buf_get_u64(ssh_buf_t *buf, uint64_t *val);
int buf_get_bool(ssh_buf_t *buf, bool *val);
int buf_get_raw(ssh_buf_t *buf, void *dst, size_t n);
int buf_get_string(ssh_buf_t *buf, uint8_t **out_data, size_t *out_len);
char *buf_get_cstring(ssh_buf_t *buf);
int buf_get_mpint(ssh_buf_t *buf, uint8_t **out_data, size_t *out_len);

#endif /* COALESCE_BUFFER_H */
