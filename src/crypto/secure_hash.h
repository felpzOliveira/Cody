/* date = December 7th 2021 21:57 */
#pragma once
#include <stdint.h>

/*
* Provides crypto-secure hash algorithms... and SHA-1.
* I'll provide SHA-1 just for utility stuff but we might not need
* since SHA-1 is very bad. SHA-3 is decent but we can do better
* so I'm not sure it is usefull, but let's do it just for the learning purposes.
* We can also generate tags using SHA-3 since it by design is suppose
* to be secure against length extension. Maybe Tag = SHA3(K | M | K)
* can be used instead of HMAC.
*/

/*
* Enables the test routine for SHA. Be aware that enabling this will attach
* around 1000 NIST test combinations for SHA-3(224/256/384/512) and SHA-1 as
* a global variable, use only for debug/tests purposes.
*/
//#define SHA_TEST

// SHA-1 Context
typedef struct SHA1Ctx{
    uint32_t e[5];
    uint32_t total[2];
    uint8_t buf[64];
}SHA1Ctx;

// SHA-3 Context
typedef struct SHA3Ctx{
    uint8_t A[200]; // state is 5x5xω, but ω=b/25 and max b=1600, so state is 200=1600/8
    uint32_t len;
    uint32_t roundsize, loc;
}SHA3Ctx;


// SHA family of functions: SHA-1 and SHA-3.

/*
* Initializes a SHA1 context for a new operation.
*/
void SHA1_Init(SHA1Ctx &ctx);

/*
* Consumes data for hashing with a SHA1 context.
*/
void SHA1_Update(SHA1Ctx &ctx, uint8_t *buf, uint32_t ilen);

/*
* Finishes the SHA1 computation and return its hash.
*/
void SHA1_Final(SHA1Ctx &ctx, uint8_t out[20]);

/*
* Perform SHA1 computation on the given buffer. This simply runs:
*    SHA1_Init(...);
*    SHA1_Update(...);
*    SHA1_Final(...);
*/
void SHA1(uint8_t *buf, uint32_t ilen, uint8_t out[20]);

/*
* Initializes a SHA3 context for a new operation. SHA-3 is the same algorithm
* for all digest sizes so let size be a parameter.
*/
void SHA3_Init(SHA3Ctx &ctx, uint32_t len);

/*
* Consumes data for hashing with a SHA3 context.
*/
void SHA3_Update(SHA3Ctx &ctx, uint8_t *buf, uint32_t len);

/*
* Finishes the SHA3 computation and return its hash.
*/
void SHA3_Final(SHA3Ctx &ctx, uint8_t *out);

/*
* Perform SHA3 computation on the given buffer. This simply runs:
*    SHA3_Init(...);
*    SHA3_Update(...);
*    SHA3_Final(...);
* Possible mdlen's are: 28 (224 bits), 32 (256 bits), 48 (384 bits), 64 (512 bits).
*/
void SHA3(uint8_t *buf, uint32_t len, uint32_t mdlen, uint8_t *out);

/*
* Simple wrapper for SHA-3 with 224 bit hash. 'out' should be 28 bytes long.
*/
void SHA3_224(uint8_t *buf, uint32_t len, uint8_t *out);

/*
* Simple wrapper for SHA-3 with 256 bit hash. 'out' should be 32 bytes long.
*/
void SHA3_256(uint8_t *buf, uint32_t len, uint8_t *out);

/*
* Simple wrapper for SHA-3 with 384 bit hash. 'out' should be 48 bytes long.
*/
void SHA3_384(uint8_t *buf, uint32_t len, uint8_t *out);

/*
* Simple wrapper for SHA-3 with 512 bit hash. 'out' should be 64 bytes long.
*/
void SHA3_512(uint8_t *buf, uint32_t len, uint8_t *out);

/*
* Computes a MAC tag based on sandwich method with SHA-3. Theoretically doing
* tag = SHA3(Pre | M | Suf) should be secure enough for authentication, if we
* ever feel it is not we can directly compute KMAC with SHAKE256.
*/
void MAC_SHA3_512(uint8_t *buf, uint8_t *prefix, uint8_t *suffix,
                  uint32_t buflen, uint32_t plen, uint32_t slen, uint8_t *out);

/*
* Run NIST test vectors on current implementation of SHA-1 and SHA-3.
* Code must be compiled with SHA_TEST.
*/
void SHA_RunTestVector();
