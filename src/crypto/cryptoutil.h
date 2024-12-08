/* date = October 25th 2021 22:18 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>

#define CRYPTO_SALT_LEN 16
#define CRYPTO_MAGIC 0x54, 0x67, 0x32, (uint8_t)'E', (uint8_t)'N', (uint8_t)'C', (uint8_t)';'

#define CRYPTO_BYTE_0( x ) ( (uint8_t) (   ( x )         & 0xff ) )
#define CRYPTO_BYTE_1( x ) ( (uint8_t) ( ( ( x ) >> 8  ) & 0xff ) )
#define CRYPTO_BYTE_2( x ) ( (uint8_t) ( ( ( x ) >> 16 ) & 0xff ) )
#define CRYPTO_BYTE_3( x ) ( (uint8_t) ( ( ( x ) >> 24 ) & 0xff ) )
#define CRYPTO_BYTE_4( x ) ( (uint8_t) ( ( ( x ) >> 32 ) & 0xff ) )
#define CRYPTO_BYTE_5( x ) ( (uint8_t) ( ( ( x ) >> 40 ) & 0xff ) )
#define CRYPTO_BYTE_6( x ) ( (uint8_t) ( ( ( x ) >> 48 ) & 0xff ) )
#define CRYPTO_BYTE_7( x ) ( (uint8_t) ( ( ( x ) >> 56 ) & 0xff ) )

#define CRYPTO_PUT_UINT32_BE( n, data, offset )                \
{                                                               \
    ( data )[( offset )    ] = CRYPTO_BYTE_3( n );             \
    ( data )[( offset ) + 1] = CRYPTO_BYTE_2( n );             \
    ( data )[( offset ) + 2] = CRYPTO_BYTE_1( n );             \
    ( data )[( offset ) + 3] = CRYPTO_BYTE_0( n );             \
}

/*
 * Encode a buffer to a base64 string.
 */
void CryptoUtil_BufferToBase64(unsigned char *buffer, size_t size, std::string &str);

/*
 * Encode a buffer to a hex string.
 */
void CryptoUtil_BufferToHex(unsigned char *buffer, size_t size, std::string &str);

/*
 * Decode a buffer from a base64 string.
 */
void CryptoUtil_BufferFromBase64(std::vector<unsigned char> &out, char *data, size_t len);

/*
 * Decode a buffer from a hex string.
 */
void CryptoUtil_BufferFromHex(std::vector<unsigned char> &out, char *data, size_t len);

/*
 * Rotates a buffer one position left.
 */
void CryptoUtil_RotateBufferLeft(unsigned char *buffer, size_t len);
void CryptoUtil_RotateBufferLeft(unsigned char **buffer, size_t len);

/*
 * Rotates a buffer one position right.
 */
void CryptoUtil_RotateBufferRight(unsigned char *buffer, size_t len);
void CryptoUtil_RotateBufferRight(unsigned char **buffer, size_t len);

/*
* Generates a key from a password. Returned key is 32 bytes in size, i.e.:
* keybuffer must have at least 32 bytes in size. Salt must be 16 bytes in size
* and should already be filled.
*/
void CryptoUtil_PasswordHash(char *password, size_t len, uint8_t *keybuffer,
                             uint8_t *salt);

/*
 * Debug. Prints the contents of a buffer in 16 block hex format.
 */
void CryptoUtil_DebugPrintBuffer(unsigned char *buffer, size_t len);
