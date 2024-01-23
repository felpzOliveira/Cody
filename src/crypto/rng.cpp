#include <rng.h>
#include <stdlib.h>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/random.h>
#include <errno.h>

void Crypto_InitRNGEngine(){}

// For linux we need to check the entropy counter first to see
// if there was actually a register. I don't ever see this failing
// but do it 'correctly'.
static int linux_rng_safe(void *buffer, size_t size){
    struct stat st;
    size_t i = 0;
    int fd, cnt, flags;
    int curr_errno = errno;
begin:
    flags = O_RDONLY | O_NOFOLLOW | O_CLOEXEC;
    fd = open("/dev/urandom", flags, 0);
    if(fd == -1){
        if(errno == EINTR){
            goto begin;
        }

        goto nodev;
    }

    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
    if(fstat(fd, &st) == -1 || !S_ISCHR(st.st_mode)){
        close(fd);
        goto nodev;
    }

    if(ioctl(fd, RNDGETENTCNT, &cnt) == -1){
        close(fd);
        goto nodev;
    }

    for(i = 0; i < size;){
        size_t wanted = size - i;
        ssize_t ret = read(fd, (char *)buffer + i, wanted);
        if(ret == -1){
            if(errno == EAGAIN || errno == EINTR) continue;
            close(fd);
            goto nodev;
        }

        i += ret;
    }

    close(fd);
    errno = curr_errno;
    return 0;

nodev:
    errno = EIO;
    return -1;
}

bool Crypto_SecureRNG(void *buffer, size_t size){
    int rv = linux_rng_safe(buffer, size);
    return rv == 0;
}

#else
#include <windows.h>
#include <wincrypt.h>

static HCRYPTPROV providerHandle = 0;
void Crypto_InitRNGEngine(){
    if(!providerHandle){
        CryptAcquireContext(&providerHandle, 0, 0, PROV_RSA_FULL,
                            CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
    }
}

bool Crypto_SecureRNG(void *buffer, size_t size){
    unsigned char *ptr = (unsigned char *)buffer;
    while(size > 0){
        const DWORD len = (DWORD)(size > 1048576UL ? 1048576UL : size);
        if(!CryptGenRandom(providerHandle, len, ptr)){
            return false;
        }

        ptr += len;
        size -= len;
    }

    return true;
}
#endif
