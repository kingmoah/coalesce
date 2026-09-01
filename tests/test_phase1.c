#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/buffer.h"
#include "../src/ssh.h"

static void test_buffer_primitives(void)
{
    ssh_buf_t buf;
    assert(buf_init(&buf, 16) == 0);

    /* Test u8 */
    assert(buf_put_u8(&buf, 0x42) == 0);
    uint8_t u8_val = 0;
    assert(buf_get_u8(&buf, &u8_val) == 0);
    assert(u8_val == 0x42);

    /* Test u32 */
    buf_reset(&buf);
    assert(buf_put_u32(&buf, 0x12345678) == 0);
    uint32_t u32_val = 0;
    assert(buf_get_u32(&buf, &u32_val) == 0);
    assert(u32_val == 0x12345678);

    /* Test u64 */
    buf_reset(&buf);
    assert(buf_put_u64(&buf, 0x0102030405060708ULL) == 0);
    uint64_t u64_val = 0;
    assert(buf_get_u64(&buf, &u64_val) == 0);
    assert(u64_val == 0x0102030405060708ULL);

    /* Test bool */
    buf_reset(&buf);
    assert(buf_put_bool(&buf, true) == 0);
    assert(buf_put_bool(&buf, false) == 0);
    bool b1 = false, b2 = true;
    assert(buf_get_bool(&buf, &b1) == 0 && b1 == true);
    assert(buf_get_bool(&buf, &b2) == 0 && b2 == false);

    /* Test string */
    buf_reset(&buf);
    const char *test_str = "ssh-userauth";
    assert(buf_put_cstring(&buf, test_str) == 0);
    char *out_str = buf_get_cstring(&buf);
    assert(out_str != NULL);
    assert(strcmp(out_str, test_str) == 0);
    free(out_str);

    /* Test mpint per RFC 4251 Section 5 */
    /* Case 1: Zero */
    buf_reset(&buf);
    uint8_t zero_bytes[1] = {0x00};
    assert(buf_put_mpint(&buf, zero_bytes, 1) == 0);
    uint8_t *mp_data = NULL;
    size_t mp_len = 0;
    assert(buf_get_mpint(&buf, &mp_data, &mp_len) == 0);
    assert(mp_len == 0);
    free(mp_data);

    /* Case 2: Positive number with MSB < 0x80 (e.g. 0x09a3) */
    buf_reset(&buf);
    uint8_t val1[2] = {0x09, 0xa3};
    assert(buf_put_mpint(&buf, val1, 2) == 0);
    assert(buf_get_mpint(&buf, &mp_data, &mp_len) == 0);
    assert(mp_len == 2);
    assert(mp_data[0] == 0x09 && mp_data[1] == 0xa3);
    free(mp_data);

    /* Case 3: Positive number with MSB >= 0x80 (e.g. 0x9a3b -> needs 0x00 prefix) */
    buf_reset(&buf);
    uint8_t val2[2] = {0x9a, 0x3b};
    assert(buf_put_mpint(&buf, val2, 2) == 0);
    assert(buf_get_mpint(&buf, &mp_data, &mp_len) == 0);
    assert(mp_len == 3);
    assert(mp_data[0] == 0x00 && mp_data[1] == 0x9a && mp_data[2] == 0x3b);
    free(mp_data);

    buf_free(&buf);
    printf("PASS: Buffer unit tests completed successfully.\n");
}

int main(void)
{
    printf("Running Phase 1 Unit Tests...\n");
    test_buffer_primitives();
    return 0;
}
