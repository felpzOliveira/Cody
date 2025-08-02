/* date = October 25th 2021 22:15 */
#pragma once
#include <stddef.h>
#include <vector>
#include <stdint.h>

/*
 * While this implementation is working nicely, it is very slow as it
 * not optmized and doesnt use assembly. Passing large files through
 * the server interface can be annoyingly slow. Maybe integrate a bit
 * of OpenSSL just so we can speed this thing up.
 */

#define AES_BLOCK_SIZE_IN_BYTES 16

/*
 * Enables the test routine for AES. Be aware that enabling this will attach
 * around 1500 NIST test combinations for AES-CBC as a global variable, use only
 * for debug/tests purposes.
 */
//#define AES_TEST

typedef enum{
    AES128, AES192, AES256
}AesKeyLength;

/*
 * Generates a AES key with a given size, basically a random array.
 * The random values are generated through the best available RNG,
 * in linux this means extracting entropy from /dev/urandom and reading
 * the bytes required. For windows we use the crypto interface provided
 * by windows with CryptGenRandom.
 */
bool AES_GenerateKey(void *buffer, AesKeyLength length);

/*
 * Encrypts a sequence of bytes using AES-CBC with a specific key. Passing
 * an already filled iv vector makes this routine uses it for encryption, if
 * the iv vector is empty or has an incompatible size than a new iv is generated
 * and returned in it.
 *
 * CBC Encryption using AES as block cipher with PKCS#7 padding.
 * Algorithm is straightforward:
 *     - Xor the first block with the iv and encrypt it;
 *     - For all other blocks xor the block with the previous result and encrypt it;
 *     - Compute the padding byte as p = n - n % b;
 *     - Introduce the padding byte into a new block or fill the last block with p;
 *     - Encrypt the last block with padded byte.
 */
bool AES_CBC_Encrypt(uint8_t *input, size_t len, uint8_t *key, AesKeyLength length,
                     std::vector<uint8_t> &out, std::vector<uint8_t> &iv);

/*
 * Decrypts a sequence of bytes previously encrypted with AES-CBC.
 *
 * CBC Decryption using AES as block cipher with PKCS#7 padding.
 * Algorithm is straightforward the reverse of encryption:
 *     - Decrypt the first block and set Si = iv;
 *     - Xor the decrypted result with Si and make Si = encrypted block i+1;
 *     - For all other blocks decrypt it and Xor with Si and make Si = encrypted block i+1;
 *     - Once finished check the last byte p and check the last p bytes are p:
 *         - In case they are the padding is correct and remove the last p bytes;
 *         - In case it is not than the message has incorrect PKCS#7 padding.
 */
bool AES_CBC_Decrypt(uint8_t *input, size_t len, uint8_t *key, uint8_t *iv,
                     AesKeyLength length, std::vector<uint8_t> &out);

/*
 * Returns the length required to encrypt data of a given size.
 */
uint32_t AES_CBC_ComputeRequiredLength(uint32_t size);

/*
 * Runs NIST test vectors on current implementation of AES-CBC.
 * Code must be compiled with AES_TEST.
 */
void AES_RunTestVector();
