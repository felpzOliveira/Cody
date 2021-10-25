/* date = June 10th 2021 11:30 */
#pragma once
#include <stdint.h>
#include <cstddef>

/*
 * Bignum interface is taken from mbedtls with some wrappers,
 * the interface itself is very generic but I'll make it use 32 bits
 * to datatype as it simplifies the code. Mbedtls implementation is very
 * interesting, it works by splitting a number into 'limbs' (uintt) and
 * chaining operations.
 */

typedef uint32_t uintt;
typedef int32_t intt;
typedef uint64_t udbl;

typedef struct Bignum{
    int s;
    size_t n;
    uintt *p;
}Bignum;

void Bignum_Init(Bignum *bn);
void Bignum_Free(Bignum *bn);

/*
 * Converts a string to a bignum given a basis 'radix'.
 */
void Bignum_FromString(Bignum *bn, const char *str, int radix);

/*
 * Writes a bignum as a string with a given basis 'radix'.
 */
void Bignum_ToString(Bignum *bn, char *str, size_t buflen, size_t *olen, int radix);

/*
 * Sum two bignum: C = A + B.
 */
void Bignum_Add(Bignum *C, Bignum *A, Bignum *B);

/*
 * Subtract two bignum: C = A - B.
 */
void Bignum_Sub(Bignum *C, Bignum *A, Bignum *B);
