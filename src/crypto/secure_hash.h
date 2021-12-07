/* date = December 7th 2021 21:57 */
#pragma once
#include <stdint.h>

/*
* Provides crypto-secure hash algorithms... and SHA-1.
*/

// SHA-1 Context
typedef struct SHA1Ctx{
    uint32_t e[5];
    uint32_t total[2];
    uint8_t buf[64];
}SHA1Ctx;

/*
* SHA family of functions: SHA-1 and SHA-3.
*/

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
