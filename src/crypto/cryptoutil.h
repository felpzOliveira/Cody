/* date = October 25th 2021 22:18 */
#pragma once
#include <stddef.h>
#include <string>
#include <vector>

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

/*
 * Rotates a buffer one position right.
 */
void CryptoUtil_RotateBufferRight(unsigned char *buffer, size_t len);
