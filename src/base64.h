#ifndef COALESCE_BASE64_H
#define COALESCE_BASE64_H

#include <stdint.h>
#include <stddef.h>

/* Standard base64 (RFC 4648) with padding.
 * encode: returns number of chars written (excluding NUL), or -1.
 * decode: returns number of bytes written, or -1 on invalid input. */

int base64_encode(char *dst, size_t dst_cap, const uint8_t *src, size_t src_len);
int base64_decode(uint8_t *dst, size_t dst_cap, const char *src);

#endif /* COALESCE_BASE64_H */
