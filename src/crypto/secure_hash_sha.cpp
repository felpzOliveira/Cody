#include <secure_hash.h>
#include <cryptoutil.h>
#include <stdlib.h>
#include <string.h>

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
