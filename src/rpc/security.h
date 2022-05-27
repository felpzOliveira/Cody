/* date = May 25th 2022 19:31 */
#pragma once

#include <types.h>
#include <vector>

namespace SecurityServices{
    void Start();
    bool Encrypt(uint8_t *input, size_t len, std::vector<uint8_t> &out);
    bool Decrypt(uint8_t *input, size_t len, std::vector<uint8_t> &out);
}

