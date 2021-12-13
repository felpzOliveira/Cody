#include <secure_hash.h>
#include <cryptoutil.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>

/****************************************************************/
//                          S H A - 3                           //
/****************************************************************/
#define ROTL64(x, y) (((x) << (y)) | ((x) >> (64 - (y))))

void SHA3_ConsumeBlock(SHA3Ctx &ctx){
    uint64_t *st = (uint64_t *)ctx.A;
    uint64_t bc[5], t = 0;
    int i = 0, j = 0, r = 0;

    const uint64_t rndc[] = {
        0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
        0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
        0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
        0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
        0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
        0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
        0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
        0x8000000000008080, 0x0000000080000001, 0x8000000080008008
    };

    const int rotc[] = {
        1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
        27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44
    };
    const int pnl[] = {
        10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
        15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1
    };

    for(r = 0; r < 24; r++){
        // Theta
        for(i = 0; i < 5; i++){
            bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];
        }

        for(i = 0; i < 5; i++){
            t = bc[(i + 4) % 5] ^ ROTL64(bc[(i + 1) % 5], 1);
            for (j = 0; j < 25; j += 5){
                st[j + i] ^= t;
            }
        }

        // Rho Pi
        t = st[1];
        for(i = 0; i < 24; i++){
            j = pnl[i];
            bc[0] = st[j];
            st[j] = ROTL64(t, rotc[i]);
            t = bc[0];
        }

        //  Chi
        for(j = 0; j < 25; j += 5){
            for(i = 0; i < 5; i++){
                bc[i] = st[j + i];
            }

            for(i = 0; i < 5; i++){
                st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
            }
        }
        //  Iota
        st[0] ^= rndc[r];
    }

    for(i = 0; i < 5; i++){
        bc[i] = 0;
    }

    t = i = j = r = 0;
}

void SHA3_Init(SHA3Ctx &ctx, uint32_t len){
    uint64_t *q = (uint64_t *)ctx.A;
    ctx.len = len;
    ctx.roundsize = 200 - 2 * len;
    ctx.loc = 0;
    for(int i = 0; i < 25; i++){
        q[i] = 0;
    }
}

void SHA3_Update(SHA3Ctx &ctx, uint8_t *buf, uint32_t len){
    uint32_t j = ctx.loc;
    for(uint32_t i = 0; i < len; i++){
        // update the state A
        ctx.A[j++] ^= buf[i];
        // execute the keccak compression in case we have a full string
        if(j >= ctx.roundsize){
            SHA3_ConsumeBlock(ctx);
            j = 0;
        }
    }

    ctx.loc = j;
}

void SHA3_Final(SHA3Ctx &ctx, uint8_t *out){
    uint64_t *q = (uint64_t *)ctx.A;
    ctx.A[ctx.loc] ^= 0x06;
    ctx.A[ctx.roundsize-1] ^= 0x80;
    SHA3_ConsumeBlock(ctx);

    for(uint32_t i = 0; i < ctx.len; i++){
        out[i] = ctx.A[i];
    }

    for(uint32_t i = 0; i < 25; i++){
        q[i] = 0;
    }
    ctx.roundsize = 0;
    ctx.loc = 0;
}

void SHA3(uint8_t *buf, uint32_t len, uint32_t mdlen, uint8_t *out){
    SHA3Ctx ctx;
    SHA3_Init(ctx, mdlen);
    SHA3_Update(ctx, buf, len);
    SHA3_Final(ctx, out);
}

void SHA3_224(uint8_t *buf, uint32_t len, uint8_t *out){
    SHA3(buf, len, 28, out);
}

void SHA3_256(uint8_t *buf, uint32_t len, uint8_t *out){
    SHA3(buf, len, 32, out);
}

void SHA3_384(uint8_t *buf, uint32_t len, uint8_t *out){
    SHA3(buf, len, 48, out);
}

void SHA3_512(uint8_t *buf, uint32_t len, uint8_t *out){
    SHA3(buf, len, 64, out);
}

/****************************************************************/

/****************************************************************/
//                          S H A - 1                           //
/****************************************************************/
#define Rot(x,n) (((x) << (n)) | (((x) & 0xFFFFFFFF) >> (32 - (n))))

static uint32_t SHA1_F(uint32_t b, uint32_t c, uint32_t d, uint32_t i){
    if(i < 20) return ((b & c) ^ (~b & d));
    if(i < 40) return (b ^ c  ^ d);
    if(i < 60) return ((b & c) ^ (b & d) ^ (c & d));
    return (b ^ c ^ d);
}

static void SHA1_ConsumeBlock(SHA1Ctx &ctx, uint8_t *block){
    uint32_t W[80]; // 64 bit expanded data
    for(int i = 0; i < 80; i++){
        if(i < 16){
            W[i] = ((uint32_t)block[4 * i + 0] << 24) |
                   ((uint32_t)block[4 * i + 1] << 16) |
                   ((uint32_t)block[4 * i + 2] << 8)  |
                   ((uint32_t)block[4 * i + 3]);
        }else{
            W[i] = Rot((W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16]), 1);
        }
    }

    uint32_t a = ctx.e[0], b = ctx.e[1], c = ctx.e[2],
             d = ctx.e[3], e = ctx.e[4];

    uint32_t k[] = {0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6};

    for(int i = 0; i < 80; i++){
        int id = i / 20;
        uint32_t s0 = Rot(a, 5) + SHA1_F(b, c, d, i) + e + k[id] + W[i];
        uint32_t s1 = a, s2 = Rot(b, 30), s3 = c, s4 = d;
        a = s0; b = s1; c = s2; d = s3; e = s4;
    }

    ctx.e[0] += a;
    ctx.e[1] += b;
    ctx.e[2] += c;
    ctx.e[3] += d;
    ctx.e[4] += e;
    memset(W, 0, sizeof(W));
    a = b = c = d = e = 0;
}

void SHA1_Init(SHA1Ctx &ctx){
    ctx.e[0] = 0x67452301;
    ctx.e[1] = 0xefcdab89;
    ctx.e[2] = 0x98badcfe;
    ctx.e[3] = 0x10325476;
    ctx.e[4] = 0xc3d2e1f0;
    ctx.total[0] = 0;
    ctx.total[1] = 0;
}

void SHA1_Update(SHA1Ctx &ctx, uint8_t *buf, uint32_t ilen){
    uint32_t left = 0, fill = 0;
    left = ctx.total[0] & 0x3f;
    fill = 64 - left;

    ctx.total[0] += ilen;
    ctx.total[0] &= 0xFFFFFFFF;
    if(ctx.total[0] < ilen){
        ctx.total[1] += 1;
    }

    if(left && ilen >= fill){
        memcpy((void *)(ctx.buf + left), buf, fill);
        SHA1_ConsumeBlock(ctx, ctx.buf);
        buf += fill;
        ilen -= fill;
        left = 0;
    }

    while(ilen >= 64){
        SHA1_ConsumeBlock(ctx, buf);
        buf += 64;
        ilen -= 64;
    }

    if(ilen > 0){
        memcpy((void *)(ctx.buf + left), buf, ilen);
    }
}

void SHA1_Final(SHA1Ctx &ctx, uint8_t out[20]){
    uint32_t used = ctx.total[0] & 0x3f;
    ctx.buf[used++] = 0x80;

    if(used <= 56){
        memset(ctx.buf + used, 0, 56 - used);
    }else{
        memset(ctx.buf + used, 0, 64 - used);
        SHA1_ConsumeBlock(ctx, ctx.buf);
        memset(ctx.buf, 0, 56);
    }

    uint32_t high = (ctx.total[0] >> 29) | (ctx.total[1] << 3);
    uint32_t low  = (ctx.total[0] << 3);

    CRYPTO_PUT_UINT32_BE(high, ctx.buf, 56);
    CRYPTO_PUT_UINT32_BE(low, ctx.buf, 60);
    SHA1_ConsumeBlock(ctx, ctx.buf);

    CRYPTO_PUT_UINT32_BE(ctx.e[0], out, 0);
    CRYPTO_PUT_UINT32_BE(ctx.e[1], out, 4);
    CRYPTO_PUT_UINT32_BE(ctx.e[2], out, 8);
    CRYPTO_PUT_UINT32_BE(ctx.e[3], out, 12);
    CRYPTO_PUT_UINT32_BE(ctx.e[4], out, 16);
    memset(&ctx, 0, sizeof(SHA1Ctx));
}

void SHA1(uint8_t *buf, uint32_t ilen, uint8_t out[20]){
    SHA1Ctx ctx;
    SHA1_Init(ctx);
    SHA1_Update(ctx, buf, ilen);
    SHA1_Final(ctx, out);
}

/****************************************************************/

void MAC_SHA3_512(uint8_t *buf, uint8_t *prefix, uint8_t *suffix,
                  uint32_t buflen, uint32_t plen, uint32_t slen, uint8_t *out)
{
    uint8_t *C = AllocatorGetN(uint8_t, buflen + plen + slen);
    uint32_t at = 0;
    if(prefix){
        memcpy(C, prefix, plen * sizeof(uint8_t));
        at += plen;
    }

    memcpy(&C[at], buf, buflen * sizeof(uint8_t));
    at += buflen;

    if(suffix){
        memcpy(&C[at], suffix, slen * sizeof(uint8_t));
        at += slen;
    }

    SHA3_512(C, at, out);

    memset(C, 0, sizeof(uint8_t) * (buflen + plen + slen));
    at = 0;

    AllocatorFree(C);
}

#if defined(SHA_TEST)

typedef struct NistShaTestEntry{
    uint32_t len;
    std::string msg;
    uint32_t bits;
    std::string md;
}NistShaTestEntry;

std::vector<NistShaTestEntry> nistShaVectors = {
    #include <test_vectors/sha1>
    #include <test_vectors/sha1_long>
    #include <test_vectors/sha3_224>
    #include <test_vectors/sha3_224_long>
    #include <test_vectors/sha3_256>
    #include <test_vectors/sha3_256_long>
    #include <test_vectors/sha3_384>
    #include <test_vectors/sha3_384_long>
    #include <test_vectors/sha3_512>
    #include <test_vectors/sha3_512_long>
};

void SHA_RunTestVector(){
    int c = 1;
    for(NistShaTestEntry &e : nistShaVectors){
        std::vector<unsigned char> md, msg;
        uint8_t out[64];
        uint32_t len = e.len;
        uint32_t bits = e.bits;
        uint32_t olen = 64;
        std::string res;
        CryptoUtil_BufferFromHex(md, (char *)e.md.c_str(), e.md.size());
        CryptoUtil_BufferFromHex(msg, (char *)e.msg.c_str(), e.msg.size());

        printf(" ==== TEST SHA [%d] ==== \n", c++);
        printf("Msg:  %s\n", e.msg.c_str());
        printf("Md:   %s\n", e.md.c_str());
        printf("Len:  %u\n", e.len);
        printf("Bits: %d\n", (int)bits);
        if(len != 0) len = msg.size();

        if(bits == 224){
            SHA3_224(msg.data(), len, out);
            olen = 28;
        }else if(bits == 256){
            SHA3_256(msg.data(), len, out);
            olen = 32;
        }else if(bits == 384){
            SHA3_384(msg.data(), len, out);
            olen = 48;
        }else if(bits == 512){
            SHA3_512(msg.data(), len, out);
            olen = 64;
        }else if(bits == 160){
            SHA1(msg.data(), len, out);
            olen = 20;
        }else{
            printf(" ** INVALID BIT SIZE ( %d )\n", (int)bits);
            exit(0);
        }

        CryptoUtil_BufferToHex(out, olen, res);
        printf(" > Hash: %s\n", res.c_str());
        if(res != e.md){
            printf("Size: %ld\n", msg.size());
            printf(" ** HASH FAILED\n");
            exit(0);
        }

        printf(" > OK\n");
        printf("===================\n");
    }

    printf("Passed all %d tests\n", c-1);
}
#else
void SHA_RunTestVector(){
    printf(" * Cody is not compiled with SHA_TEST definition\n");
}
#endif
