/* rand.c — OS entropy: CryptGenRandom (Windows), getrandom/urandom (Unix). */
#include "rand.h"

#ifdef _WIN32
  #include <windows.h>
  #include <wincrypt.h>
#else
  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/syscall.h>
  #include <errno.h>
#endif

int rand_bytes(uint8_t *dst, size_t len)
{
    if (!dst || len == 0) return 0;

#ifdef _WIN32
    HCRYPTPROV prov = 0;
    if (!CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
        return -1;
    BOOL ok = CryptGenRandom(prov, (DWORD)len, dst);
    CryptReleaseContext(prov, 0);
    return ok ? 0 : -1;
#else
    /* Preferred: getrandom() (Linux 3.17+, glibc 2.25+) */
#if defined(SYS_getrandom) && defined(__linux__)
    long r = syscall(SYS_getrandom, dst, len, 0);
    if (r == (long)len) return 0;
#endif
    /* Fallback: /dev/urandom */
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        size_t got = 0;
        while (got < len) {
            ssize_t r = read(fd, dst + got, len - got);
            if (r <= 0) break;
            got += (size_t)r;
        }
        close(fd);
        if (got == len) return 0;
    }
    return -1;
#endif
}
