/* date = October 25th 2021 20:17 */
#pragma once
#include <stddef.h>

/*
 * Generates crypto compatible random numbers.
 */
bool Crypto_SecureRNG(void *buffer, size_t size);
