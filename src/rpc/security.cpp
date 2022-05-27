#include <security.h>
#include <types.h>
#include <aes.h>

// TODO: This whole file is for testing and does not represent anything yet
// TODO: Key exchange with Frodo
const uint8_t key[] = { 0x19, 0x57, 0x72, 0x14, 0xf9, 0x50, 0xfd, 0x01, 0x95,
                        0x9b, 0x24, 0x4a, 0x0a, 0x05, 0x06, 0xcd, 0x5e, 0xb9,
                        0x8c, 0xc1, 0x28, 0xa4, 0x9d, 0x47, 0x02, 0xc4, 0x2a,
                        0x88, 0x0e, 0xb8, 0x65, 0xc5 };

void SecurityServices::Start(){
    // TODO: Initialize crypto stuff
}

bool SecurityServices::Encrypt(uint8_t *input, size_t len, std::vector<uint8_t> &out){
    std::vector<uint8_t> iv;
    bool rv = AES_CBC_Encrypt(input, len, (uint8_t *)key, AES256, out, iv);
    if(rv){
        for(int i = 0; i < AES_BLOCK_SIZE_IN_BYTES; i++){
            out.push_back(iv[i]);
        }
    }
    return rv;
}

bool SecurityServices::Decrypt(uint8_t *input, size_t len, std::vector<uint8_t> &out){
    uint8_t *iv = nullptr;
    uint8_t *enc = input;

    if(len <= AES_BLOCK_SIZE_IN_BYTES) return false;
    if(!input) return false;

    iv = &input[len - AES_BLOCK_SIZE_IN_BYTES];
    len -= AES_BLOCK_SIZE_IN_BYTES;

    return AES_CBC_Decrypt(enc, len, (uint8_t *)key, iv, AES256, out);
}
