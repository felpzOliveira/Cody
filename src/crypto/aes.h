/* date = October 25th 2021 22:15 */
#pragma once
#include <stddef.h>
#include <vector>
#include <stdint.h>

/*
 * Disclaimer: All crypto code present in Cody is meant to be both
 * learn material for myself and provide a sandbox for basic operations.
 * This means that while I try my best to make this code safe it is meant
 * to protect against connection stuff and not local stuff, i.e.: I won't
 * bother with things like: caching attack and other side channels attacks
 * as for this application it makes no sense to worry about that and it would
 * only make it harder to implement these algorithms. I'm a strong believer
 * on 2 things:
 *   1 - We should consider implementing things ourselves, it's the only way
 *       we learn and maybe get things to a better place;
 *   2 - Applications should use what they need and not fantasize about
 *       things that are impossible in their scenarios.
 * This implies that my implementation is meant to provide protection over network
 * connections and not on local machines, If you feel like this code should
 * do more I suggest you implement crypto using either OpenSSL or MbedTLS.
 * I also don't make any promises that this will be decently fast, performance
 * is also irrelevant for Cody as the crypto agent will be detached and the editor's
 * performance won't be affect. I'll however eventually tabulate things but only when
 * actually required. While I have written several protocols around OpenSSL and MbedTLS
 * I have never written the crypto stuff at low level, so expect the implementation to
 * be the 'naive' one, however this is a good oportunity to learn implementation details
 * of each algorithm.
 */

/*
 * Note: The AES implementation here is the reference implementation, i.e.: it only
 * uses tables for the galois field multiplication and does not merge ShiftRows,
 * MixColumns, etc... into table components. Expect it to be slower than the optmized
 * verison you would find in OpenSSL and MbedTLS.
 */

#define AES_BLOCK_SIZE_IN_BYTES 16

/*
 * Enables the test routine for AES. Be aware that enabling this will attach
 * around 1500 NIST test combinations for AES-CBC as a global variable, use only
 * for debug/tests purposes.
 */
#define AES_TEST

typedef enum{
    AES128, AES192, AES256
}AesKeyLength;

/*
 * Generates a AES key with a given size, basically a random array.
 */
bool AES_GenerateKey(void *buffer, AesKeyLength length);

/*
 * Encrypts a sequence of bytes using AES-CBC with a specific key. Passing
 * an already filled iv vector makes this routine uses it for encryption, if
 * the iv vector is empty than a new iv is generated and returned in it.
 */
bool AES_CBC_Encrypt(uint8_t *input, size_t len, uint8_t *key, AesKeyLength length,
                     std::vector<uint8_t> &out, std::vector<uint8_t> &iv);

/*
 * Decrypts a sequence of bytes previously encrypted with AES-CBC.
 */
bool AES_CBC_Decrypt(uint8_t *input, size_t len, uint8_t *key, uint8_t *iv,
                     AesKeyLength length, std::vector<uint8_t> &out);

/*
 * Runs NIST test vectors on current implementation of AES-CBC.
 * Code must be compiled with AES_TEST.
 */
void AES_RunTestVector();
