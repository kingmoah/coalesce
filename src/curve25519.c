#include "curve25519.h"
#include <string.h>

#include "mbedtls/ecdh.h"
#include "mbedtls/ecp.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/bignum.h"

/* ── Internal helper: load a raw 32-byte little-endian scalar ─── */

static int load_scalar(mbedtls_mpi *mpi, const uint8_t scalar[CURVE25519_KEY_SIZE])
{
    /* Curve25519 scalars are stored little-endian on the wire; mbedtls MPI
     * is big-endian, so we reverse before importing. */
    uint8_t be[CURVE25519_KEY_SIZE];
    for (int i = 0; i < CURVE25519_KEY_SIZE; i++)
        be[i] = scalar[CURVE25519_KEY_SIZE - 1 - i];
    return mbedtls_mpi_read_binary(mpi, be, CURVE25519_KEY_SIZE);
}

/* ── Internal helper: store a point's x-coordinate as 32-byte LE */

static int store_point_x(uint8_t out[CURVE25519_KEY_SIZE], const mbedtls_ecp_point *P)
{
    uint8_t be[CURVE25519_KEY_SIZE];
    int ret = mbedtls_mpi_write_binary(&P->MBEDTLS_PRIVATE(X), be, CURVE25519_KEY_SIZE);
    if (ret != 0) return ret;
    /* Convert big-endian → little-endian */
    for (int i = 0; i < CURVE25519_KEY_SIZE; i++)
        out[i] = be[CURVE25519_KEY_SIZE - 1 - i];
    return 0;
}

/* ── Internal helper: load a raw 32-byte LE point (u-coordinate) */

static int load_point(mbedtls_ecp_group *grp, mbedtls_ecp_point *P,
                      const uint8_t u[CURVE25519_KEY_SIZE])
{
    /* Convert LE wire format → big-endian for mbedtls MPI */
    uint8_t be[CURVE25519_KEY_SIZE];
    for (int i = 0; i < CURVE25519_KEY_SIZE; i++)
        be[i] = u[CURVE25519_KEY_SIZE - 1 - i];

    int ret;
    if ((ret = mbedtls_mpi_read_binary(&P->MBEDTLS_PRIVATE(X), be, CURVE25519_KEY_SIZE)) != 0)
        return ret;
    /* For Montgomery curves mbedtls only uses X; set Y=1, Z=1 */
    if ((ret = mbedtls_mpi_lset(&P->MBEDTLS_PRIVATE(Y), 1)) != 0) return ret;
    if ((ret = mbedtls_mpi_lset(&P->MBEDTLS_PRIVATE(Z), 1)) != 0) return ret;
    (void)grp;
    return 0;
}

/* ── curve25519_eval: out = scalar * point ────────────────────── */

int curve25519_eval(uint8_t out[CURVE25519_KEY_SIZE],
                    const uint8_t scalar[CURVE25519_KEY_SIZE],
                    const uint8_t point[CURVE25519_KEY_SIZE])
{
    int ret = -1;
    mbedtls_ecp_group grp;
    mbedtls_ecp_point P, R;
    mbedtls_mpi k;

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&P);
    mbedtls_ecp_point_init(&R);
    mbedtls_mpi_init(&k);

    /* Apply Curve25519 clamping to a local copy of the scalar */
    uint8_t clamped[CURVE25519_KEY_SIZE];
    memcpy(clamped, scalar, CURVE25519_KEY_SIZE);
    clamped[0]  &= 248;
    clamped[31] &= 127;
    clamped[31] |= 64;

    if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE25519) != 0) goto cleanup;
    if (load_scalar(&k, clamped) != 0) goto cleanup;
    if (load_point(&grp, &P, point) != 0) goto cleanup;
    if (mbedtls_ecp_mul(&grp, &R, &k, &P, NULL, NULL) != 0) goto cleanup;
    if (store_point_x(out, &R) != 0) goto cleanup;

    ret = 0;

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&P);
    mbedtls_ecp_point_free(&R);
    mbedtls_mpi_free(&k);
    return ret;
}

/* ── curve25519_base: out = scalar * G ───────────────────────── */

int curve25519_base(uint8_t out[CURVE25519_KEY_SIZE],
                    const uint8_t scalar[CURVE25519_KEY_SIZE])
{
    static const uint8_t base_point[CURVE25519_KEY_SIZE] = {9};
    return curve25519_eval(out, scalar, base_point);
}

/* ── curve25519_generate_private ─────────────────────────────── */

void curve25519_generate_private(uint8_t private_key[CURVE25519_KEY_SIZE])
{
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "coalesce_curve25519";

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                           (const unsigned char *)pers, strlen(pers));

    mbedtls_ctr_drbg_random(&ctr_drbg, private_key, CURVE25519_KEY_SIZE);

    /* RFC 7748 clamping */
    private_key[0]  &= 248;
    private_key[31] &= 127;
    private_key[31] |= 64;

    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}
