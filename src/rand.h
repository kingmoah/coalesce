#ifndef COALESCE_RAND_H
#define COALESCE_RAND_H

#include <stdint.h>
#include <stddef.h>

/* Fill dst with len cryptographically secure random bytes.
 * Returns 0 on success, -1 if no secure entropy source is available
 * (never falls back to a weak PRNG). */
int rand_bytes(uint8_t *dst, size_t len);

#endif /* COALESCE_RAND_H */
