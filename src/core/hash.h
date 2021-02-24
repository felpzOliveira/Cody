/* date = February 22nd 2021 5:59 pm */

#ifndef HASH_H
#define HASH_H
#include <types.h>

/*
* This file contains a few definitions of string hashing schemes for tables
* and are not to be used for cryptographic operations.
*/

/*
* Performs the Fowler/Noll/Vo hashing algorithm into the contents
* of the string given in str and returns a 32 bit hash result.
*/
uint FowlerNollVoStringHash32(char *str);

/*
* Performs the Fowler/Noll/Vo hashing algorithm into the contents
* of the string given in str and returns a 64 bit hash result.
*/
uint64 FowlerNollVoStringHash64(char *str);

/*
* Performs the MumurHash2 hashing algorithm into the contents of the
* string given in str and returns a 32 bit hash result.
*/
uint MurmurHash2(char *str, int len, uint seed);

/*
* Performs the MumurHash3 hashing algorithm into the contents of the
* string given in str and returns a 32 bit hash result.
*/
uint MurmurHash3(char *str, int len, uint seed);

#endif //HASH_H
