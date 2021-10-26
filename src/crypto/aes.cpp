#include <aes.h>
#include <rng.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <cryptoutil.h>
#include <iostream>

#define AES_BLOCK_SIZE_WINDOW 4
#define AES_BLOCK_SIZE_IN_BITS  (AES_BLOCK_SIZE_IN_BYTES << 3)
#define AES_MAX_STATE_SIZE 32
#define AES_MAX_EXPANDED_KEY_SIZE_IN_BYTES ((AES_BLOCK_SIZE_IN_BYTES) * 15)

#define AES_WORD_XOR(b0, b1)\
(b0)[0] = (b0)[0] ^ (b1)[0];\
(b0)[1] = (b0)[1] ^ (b1)[1];\
(b0)[2] = (b0)[2] ^ (b1)[2];\
(b0)[3] = (b0)[3] ^ (b1)[3];

#define AES_WORD_SET(dst, src, offset) \
(dst)[0] = (src)[4 * (offset)];\
(dst)[1] = (src)[4 * (offset) + 1];\
(dst)[2] = (src)[4 * (offset) + 2];\
(dst)[3] = (src)[4 * (offset) + 3];

#define AES_GALOIS(a, b) AES_GaloisMult(a[0], b[0]) ^ AES_GaloisMult(a[1], b[1]) ^\
                         AES_GaloisMult(a[2], b[2]) ^ AES_GaloisMult(a[3], b[3])

static uint8_t AES_SBox[256] = {
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7,
    0xAB, 0x76, 0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0, 0xAD, 0xD4, 0xA2, 0xAF,
    0x9C, 0xA4, 0x72, 0xC0, 0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5,
    0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15, 0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A,
    0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75, 0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E,
    0x5A, 0xA0, 0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84, 0x53, 0xD1, 0x00, 0xED,
    0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF, 0xD0, 0xEF,
    0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5, 0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF,
    0xF3, 0xD2, 0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D,
    0x64, 0x5D, 0x19, 0x73, 0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE,
    0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB, 0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C,
    0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79, 0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5,
    0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08, 0xBA, 0x78, 0x25, 0x2E,
    0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A, 0x70, 0x3E,
    0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E, 0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55,
    0x28, 0xDF, 0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F,
    0xB0, 0x54, 0xBB, 0x16
};

static uint8_t AES_InvSBox[256] = {
    0x52, 0x09, 0x6A, 0xD5, 0x30, 0x36, 0xA5, 0x38, 0xBF, 0x40, 0xA3, 0x9E, 0x81, 0xF3,
    0xD7, 0xFB, 0x7C, 0xE3, 0x39, 0x82, 0x9B, 0x2F, 0xFF, 0x87, 0x34, 0x8E, 0x43, 0x44,
    0xC4, 0xDE, 0xE9, 0xCB, 0x54, 0x7B, 0x94, 0x32, 0xA6, 0xC2, 0x23, 0x3D, 0xEE, 0x4C,
    0x95, 0x0B, 0x42, 0xFA, 0xC3, 0x4E, 0x08, 0x2E, 0xA1, 0x66, 0x28, 0xD9, 0x24, 0xB2,
    0x76, 0x5B, 0xA2, 0x49, 0x6D, 0x8B, 0xD1, 0x25, 0x72, 0xF8, 0xF6, 0x64, 0x86, 0x68,
    0x98, 0x16, 0xD4, 0xA4, 0x5C, 0xCC, 0x5D, 0x65, 0xB6, 0x92, 0x6C, 0x70, 0x48, 0x50,
    0xFD, 0xED, 0xB9, 0xDA, 0x5E, 0x15, 0x46, 0x57, 0xA7, 0x8D, 0x9D, 0x84, 0x90, 0xD8,
    0xAB, 0x00, 0x8C, 0xBC, 0xD3, 0x0A, 0xF7, 0xE4, 0x58, 0x05, 0xB8, 0xB3, 0x45, 0x06,
    0xD0, 0x2C, 0x1E, 0x8F, 0xCA, 0x3F, 0x0F, 0x02, 0xC1, 0xAF, 0xBD, 0x03, 0x01, 0x13,
    0x8A, 0x6B, 0x3A, 0x91, 0x11, 0x41, 0x4F, 0x67, 0xDC, 0xEA, 0x97, 0xF2, 0xCF, 0xCE,
    0xF0, 0xB4, 0xE6, 0x73, 0x96, 0xAC, 0x74, 0x22, 0xE7, 0xAD, 0x35, 0x85, 0xE2, 0xF9,
    0x37, 0xE8, 0x1C, 0x75, 0xDF, 0x6E, 0x47, 0xF1, 0x1A, 0x71, 0x1D, 0x29, 0xC5, 0x89,
    0x6F, 0xB7, 0x62, 0x0E, 0xAA, 0x18, 0xBE, 0x1B, 0xFC, 0x56, 0x3E, 0x4B, 0xC6, 0xD2,
    0x79, 0x20, 0x9A, 0xDB, 0xC0, 0xFE, 0x78, 0xCD, 0x5A, 0xF4, 0x1F, 0xDD, 0xA8, 0x33,
    0x88, 0x07, 0xC7, 0x31, 0xB1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xEC, 0x5F, 0x60, 0x51,
    0x7F, 0xA9, 0x19, 0xB5, 0x4A, 0x0D, 0x2D, 0xE5, 0x7A, 0x9F, 0x93, 0xC9, 0x9C, 0xEF,
    0xA0, 0xE0, 0x3B, 0x4D, 0xAE, 0x2A, 0xF5, 0xB0, 0xC8, 0xEB, 0xBB, 0x3C, 0x83, 0x53,
    0x99, 0x61, 0x17, 0x2B, 0x04, 0x7E, 0xBA, 0x77, 0xD6, 0x26, 0xE1, 0x69, 0x14, 0x63,
    0x55, 0x21, 0x0C, 0x7D
};

static uint8_t AES_Rcon[11] = { 0x8d, 0x01, 0x02, 0x04, 0x08, 0x10,
                                0x20, 0x40, 0x80, 0x1B, 0x36 };

static uint8_t AES_L[256] = {
    0x00, 0x00, 0x19, 0x01, 0x32, 0x02, 0x1A, 0xC6, 0x4B, 0xC7, 0x1B, 0x68, 0x33, 0xEE,
    0xDF, 0x03, 0x64, 0x04, 0xE0, 0x0E, 0x34, 0x8D, 0x81, 0xEF, 0x4C, 0x71, 0x08, 0xC8,
    0xF8, 0x69, 0x1C, 0xC1, 0x7D, 0xC2, 0x1D, 0xB5, 0xF9, 0xB9, 0x27, 0x6A, 0x4D, 0xE4,
    0xA6, 0x72, 0x9A, 0xC9, 0x09, 0x78, 0x65, 0x2F, 0x8A, 0x05, 0x21, 0x0F, 0xE1, 0x24,
    0x12, 0xF0, 0x82, 0x45, 0x35, 0x93, 0xDA, 0x8E, 0x96, 0x8F, 0xDB, 0xBD, 0x36, 0xD0,
    0xCE, 0x94, 0x13, 0x5C, 0xD2, 0xF1, 0x40, 0x46, 0x83, 0x38, 0x66, 0xDD, 0xFD, 0x30,
    0xBF, 0x06, 0x8B, 0x62, 0xB3, 0x25, 0xE2, 0x98, 0x22, 0x88, 0x91, 0x10, 0x7E, 0x6E,
    0x48, 0xC3, 0xA3, 0xB6, 0x1E, 0x42, 0x3A, 0x6B, 0x28, 0x54, 0xFA, 0x85, 0x3D, 0xBA,
    0x2B, 0x79, 0x0A, 0x15, 0x9B, 0x9F, 0x5E, 0xCA, 0x4E, 0xD4, 0xAC, 0xE5, 0xF3, 0x73,
    0xA7, 0x57, 0xAF, 0x58, 0xA8, 0x50, 0xF4, 0xEA, 0xD6, 0x74, 0x4F, 0xAE, 0xE9, 0xD5,
    0xE7, 0xE6, 0xAD, 0xE8, 0x2C, 0xD7, 0x75, 0x7A, 0xEB, 0x16, 0x0B, 0xF5, 0x59, 0xCB,
    0x5F, 0xB0, 0x9C, 0xA9, 0x51, 0xA0, 0x7F, 0x0C, 0xF6, 0x6F, 0x17, 0xC4, 0x49, 0xEC,
    0xD8, 0x43, 0x1F, 0x2D, 0xA4, 0x76, 0x7B, 0xB7, 0xCC, 0xBB, 0x3E, 0x5A, 0xFB, 0x60,
    0xB1, 0x86, 0x3B, 0x52, 0xA1, 0x6C, 0xAA, 0x55, 0x29, 0x9D, 0x97, 0xB2, 0x87, 0x90,
    0x61, 0xBE, 0xDC, 0xFC, 0xBC, 0x95, 0xCF, 0xCD, 0x37, 0x3F, 0x5B, 0xD1, 0x53, 0x39,
    0x84, 0x3C, 0x41, 0xA2, 0x6D, 0x47, 0x14, 0x2A, 0x9E, 0x5D, 0x56, 0xF2, 0xD3, 0xAB,
    0x44, 0x11, 0x92, 0xD9, 0x23, 0x20, 0x2E, 0x89, 0xB4, 0x7C, 0xB8, 0x26, 0x77, 0x99,
    0xE3, 0xA5, 0x67, 0x4A, 0xED, 0xDE, 0xC5, 0x31, 0xFE, 0x18, 0x0D, 0x63, 0x8C, 0x80,
    0xC0, 0xF7, 0x70, 0x07,
};

static uint8_t AES_E[256] = {
    0x01, 0x03, 0x05, 0x0F, 0x11, 0x33, 0x55, 0xFF, 0x1A, 0x2E, 0x72, 0x96, 0xA1, 0xF8,
    0x13, 0x35, 0x5F, 0xE1, 0x38, 0x48, 0xD8, 0x73, 0x95, 0xA4, 0xF7, 0x02, 0x06, 0x0A,
    0x1E, 0x22, 0x66, 0xAA, 0xE5, 0x34, 0x5C, 0xE4, 0x37, 0x59, 0xEB, 0x26, 0x6A, 0xBE,
    0xD9, 0x70, 0x90, 0xAB, 0xE6, 0x31, 0x53, 0xF5, 0x04, 0x0C, 0x14, 0x3C, 0x44, 0xCC,
    0x4F, 0xD1, 0x68, 0xB8, 0xD3, 0x6E, 0xB2, 0xCD, 0x4C, 0xD4, 0x67, 0xA9, 0xE0, 0x3B,
    0x4D, 0xD7, 0x62, 0xA6, 0xF1, 0x08, 0x18, 0x28, 0x78, 0x88, 0x83, 0x9E, 0xB9, 0xD0,
    0x6B, 0xBD, 0xDC, 0x7F, 0x81, 0x98, 0xB3, 0xCE, 0x49, 0xDB, 0x76, 0x9A, 0xB5, 0xC4,
    0x57, 0xF9, 0x10, 0x30, 0x50, 0xF0, 0x0B, 0x1D, 0x27, 0x69, 0xBB, 0xD6, 0x61, 0xA3,
    0xFE, 0x19, 0x2B, 0x7D, 0x87, 0x92, 0xAD, 0xEC, 0x2F, 0x71, 0x93, 0xAE, 0xE9, 0x20,
    0x60, 0xA0, 0xFB, 0x16, 0x3A, 0x4E, 0xD2, 0x6D, 0xB7, 0xC2, 0x5D, 0xE7, 0x32, 0x56,
    0xFA, 0x15, 0x3F, 0x41, 0xC3, 0x5E, 0xE2, 0x3D, 0x47, 0xC9, 0x40, 0xC0, 0x5B, 0xED,
    0x2C, 0x74, 0x9C, 0xBF, 0xDA, 0x75, 0x9F, 0xBA, 0xD5, 0x64, 0xAC, 0xEF, 0x2A, 0x7E,
    0x82, 0x9D, 0xBC, 0xDF, 0x7A, 0x8E, 0x89, 0x80, 0x9B, 0xB6, 0xC1, 0x58, 0xE8, 0x23,
    0x65, 0xAF, 0xEA, 0x25, 0x6F, 0xB1, 0xC8, 0x43, 0xC5, 0x54, 0xFC, 0x1F, 0x21, 0x63,
    0xA5, 0xF4, 0x07, 0x09, 0x1B, 0x2D, 0x77, 0x99, 0xB0, 0xCB, 0x46, 0xCA, 0x45, 0xCF,
    0x4A, 0xDE, 0x79, 0x8B, 0x86, 0x91, 0xA8, 0xE3, 0x3E, 0x42, 0xC6, 0x51, 0xF3, 0x0E,
    0x12, 0x36, 0x5A, 0xEE, 0x29, 0x7B, 0x8D, 0x8C, 0x8F, 0x8A, 0x85, 0x94, 0xA7, 0xF2,
    0x0D, 0x17, 0x39, 0x4B, 0xDD, 0x7C, 0x84, 0x97, 0xA2, 0xFD, 0x1C, 0x24, 0x6C, 0xB4,
    0xC7, 0x52, 0xF6, 0x01,
};

typedef struct{
    uint8_t state[AES_MAX_STATE_SIZE]; // state of the aes at any point
    uint8_t expandedKey[AES_MAX_EXPANDED_KEY_SIZE_IN_BYTES]; // expanded key, blockSize * (rounds+1)
    unsigned int blockSize; // block size, mostly 16
    unsigned int rounds; // number of rounds, depend on key size
    unsigned int keySize; // key size in bytes
    unsigned int columns; // columns in the AES state, i.e.: blockSize / 32
}AesContext;

static void AES_CopyTransposed(uint8_t *ptr, uint8_t *out){
    int c = 0;
    for(int i = 0; i < AES_BLOCK_SIZE_WINDOW; i++){
        for(int j = 0; j < AES_BLOCK_SIZE_WINDOW; j++){
            int a = AES_BLOCK_SIZE_WINDOW * j + i;
            ptr[a] = out[c++];
        }
    }
}

static unsigned int AES_KeySizeInBytes(AesKeyLength length){
    unsigned int size = 0;
    switch(length){
        case AES128: size = 16; break;
        case AES192: size = 24; break;
        case AES256: size = 32; break;
    }

    return size;
}

static unsigned int AES_NumberOfRounds(AesKeyLength length){
    unsigned int rounds = 0;
    switch(length){
        case AES128: rounds = 10; break;
        case AES192: rounds = 12; break;
        case AES256: rounds = 14; break;
    }

    return rounds;
}

static void AES_SubByte(uint8_t *buffer, size_t len, bool inverse=false){
    for(size_t i = 0; i < len; i++){
        if(inverse){
            buffer[i] = AES_InvSBox[buffer[i]];
        }else{
            buffer[i] = AES_SBox[buffer[i]];
        }
    }
}

static uint8_t AES_GaloisMult(uint8_t a, uint8_t b){
    if(a == 0 || b == 0) return 0;

    uint32_t s = AES_L[a] + AES_L[b];
    s = s > 0xFF ? s - 0xFF : s;
    return AES_E[s];
}

static void AES_InitializeContext(AesContext *ctx, AesKeyLength length){
    ctx->keySize = AES_KeySizeInBytes(length);
    ctx->blockSize = AES_BLOCK_SIZE_IN_BYTES;
    ctx->rounds = AES_NumberOfRounds(length);
    ctx->columns = (AES_BLOCK_SIZE_IN_BITS >> 5);
    memset(ctx->state, 0, AES_MAX_STATE_SIZE);
    memset(ctx->expandedKey, 0, AES_MAX_EXPANDED_KEY_SIZE_IN_BYTES);
}

static bool AES_KeyExpansion(AesContext *ctx, uint8_t *key){
    unsigned int size = ctx->keySize;
    unsigned int Nb = ctx->columns;
    unsigned int Nk = (size * 8) >> 5; // size of each piece is key size / 32
    int it = Nb * (1 + ctx->rounds);
    uint8_t *exp = ctx->expandedKey;
    uint8_t tp[4], ss[4];

    // 1 - Copy the initial key to the expanded key first round key address K0
    memcpy(exp, key, size);

    // 2 - For each Nb bytes following apply expansion. Accoding to AES Proposal
    //     Nb = BlockSize / 32, which for us is in ctx->columns, the fact that
    //     the original key lies in the first bytes does not matter. It doesn't
    //     seem to have any relation between the key size and the expanded key
    //     Nb length. Iteration however begins at key size / 32.
    for(int i = Nk; i < it; i++){
        AES_WORD_SET(tp, exp, i-1);
        AES_WORD_SET(ss, exp, i-Nk);
        if(i % Nk == 0){
            unsigned int rcon = i / Nk;
            CryptoUtil_RotateBufferLeft(tp, Nb);
            AES_SubByte(tp, 4);
            tp[0] ^= AES_Rcon[rcon];
        }else if(Nk > 6 && i % 4 == 0){
            AES_SubByte(tp, 4);
        }

        AES_WORD_XOR(tp, ss);
        AES_WORD_SET(&exp[4 * i], tp, 0);
    }

    memset(tp, 0, 4);
    memset(ss, 0, 4);

    return true;
}

static void AES_AddRoundKey(uint8_t *state, uint8_t *key){
#if defined(AES_DEBUG)
    std::string e0, e1, e2;
    CryptoUtil_BufferToHex(state, 16, e0);
    CryptoUtil_BufferToHex(key, 16, e1);
#endif
    for(int i = 0; i < AES_BLOCK_SIZE_IN_BYTES; i++){
        state[i] ^= key[i];
    }

#if defined(AES_DEBUG)
    CryptoUtil_BufferToHex(state, 16, e2);
    std::cout << "[RoundKey]   " << e0 << " x " << e1 << " = " << e2 << std::endl;
#endif
}

static void AES_MixColumns(uint8_t *state, bool inverse=false){
    uint8_t resp[AES_BLOCK_SIZE_IN_BYTES];
    uint8_t forwardMatrix[] = {
        0x02, 0x03, 0x01, 0x01,
        0x01, 0x02, 0x03, 0x01,
        0x01, 0x01, 0x02, 0x03,
        0x03, 0x01, 0x01, 0x02,
    };

    uint8_t inverseMatrix[] = {
        0x0E, 0x0B, 0x0D, 0x09,
        0x09, 0x0E, 0x0B, 0x0D,
        0x0D, 0x09, 0x0E, 0x0B,
        0x0B, 0x0D, 0x09, 0x0E,
    };

#if defined(AES_DEBUG)
    std::string e0;
    CryptoUtil_BufferToHex(state, 16, e0);
#endif
    for(int i = 0; i < 4; i++){
        uint8_t col[] = { state[4 * 0 + i], state[4 * 1 + i],
                          state[4 * 2 + i], state[4 * 3 + i] };
        for(int j = 0; j < 4; j++){
            uint8_t *row;
            if(inverse){
                row = &inverseMatrix[4 * j];
            }else{
                row = &forwardMatrix[4 * j];
            }
            resp[4 * j + i] = AES_GALOIS(col, row);
        }
    }

    memcpy(state, resp, 16);
#if defined(AES_DEBUG)
    std::string e1;
    CryptoUtil_BufferToHex(state, 16, e1);
    std::cout << "[MixColumns] " << e0 << " => " << e1 << std::endl;
#endif
}

static void AES_ShiftRows(uint8_t *state, bool inverse=false){
#if defined(AES_DEBUG)
    std::string e0, e1;
    CryptoUtil_BufferToHex(state, 16, e0);
#endif

    for(int i = 1; i < 4; i++){
        uint8_t *row[] = { &state[4 * i + 0], &state[4 * i + 1],
                           &state[4 * i + 2], &state[4 * i + 3] };
        for(int j = 0; j < i; j++){
            if(inverse){
                CryptoUtil_RotateBufferRight((unsigned char **)row, 4);
            }else{
                CryptoUtil_RotateBufferLeft((unsigned char **)row, 4);
            }
        }
    }

#if defined(AES_DEBUG)
    CryptoUtil_BufferToHex(state, 16, e1);
    std::cout << "[ShiftRows]  " << e0 << " = " << e1 << std::endl;
#endif
}

static void AES_BlockEncrypt(uint8_t *block, uint8_t *output, AesContext *ctx){
    uint8_t RK[AES_BLOCK_SIZE_IN_BYTES];
    // 1 - The initial AES context must be initialized from the 'plaintext'
    AES_CopyTransposed(RK, ctx->expandedKey);
    AES_CopyTransposed(ctx->state, block);

    // 2 - Add initial round key, part of the original user key
    AES_AddRoundKey(ctx->state, RK);

    for(unsigned int i = 1; i <= ctx->rounds; i++){
        // Run rounds
        AES_CopyTransposed(RK, &ctx->expandedKey[i * AES_BLOCK_SIZE_IN_BYTES]);

        AES_SubByte(ctx->state, AES_BLOCK_SIZE_IN_BYTES);

        AES_ShiftRows(ctx->state);

        if(i != ctx->rounds){
            AES_MixColumns(ctx->state);
        }

        AES_AddRoundKey(ctx->state, RK);
    }

    // Copy output block
    AES_CopyTransposed(output, ctx->state);
}

static void AES_BlockDecrypt(uint8_t *block, uint8_t *output, AesContext *ctx){
    uint8_t RK[AES_BLOCK_SIZE_IN_BYTES];
    // 1 - The initial AES context must be initialized from the 'plaintext'
    AES_CopyTransposed(RK, &ctx->expandedKey[ctx->rounds * AES_BLOCK_SIZE_IN_BYTES]);
    AES_CopyTransposed(ctx->state, block);

    // 2- Add initial round key, backwards
    AES_AddRoundKey(ctx->state, RK);

    for(int i = ctx->rounds-1; i >= 0; i--){
        // Run rounds
        AES_CopyTransposed(RK, &ctx->expandedKey[i * AES_BLOCK_SIZE_IN_BYTES]);

        AES_ShiftRows(ctx->state, true);

        AES_SubByte(ctx->state, AES_BLOCK_SIZE_IN_BYTES, true);

        AES_AddRoundKey(ctx->state, RK);

        if(i != 0){
            AES_MixColumns(ctx->state, true);
        }

    }

    // Copy output block
    AES_CopyTransposed(output, ctx->state);
}

bool AES_GenerateKey(void *buffer, AesKeyLength length){
    unsigned int size = AES_KeySizeInBytes(length);
    if(size == 0){
        printf("Invalid key size\n");
        return false;
    }
    return Crypto_SecureRNG(buffer, size);
}

bool AES_CBC_Decrypt(uint8_t *input, size_t len, uint8_t *key, uint8_t *iv,
                     AesKeyLength length, std::vector<uint8_t> &out)
{
    AesContext ctx;
    uint8_t block[AES_BLOCK_SIZE_IN_BYTES];
    uint8_t dec[AES_BLOCK_SIZE_IN_BYTES];
    size_t head = 0;
    uint8_t *ci1 = &iv[0];

    AES_InitializeContext(&ctx, length);
    if(ctx.keySize == 0){
        return false;
    }

    if(len % AES_BLOCK_SIZE_IN_BYTES != 0 || len == 0){
        printf("Not compatible PKCS#7 padding\n");
        return false;
    }

    AES_KeyExpansion(&ctx, (uint8_t *)key);

    while(len > 0){
        // get the ciphered block
        memcpy(block, &input[head], AES_BLOCK_SIZE_IN_BYTES);
        len -= AES_BLOCK_SIZE_IN_BYTES;

        // decrypt it
        AES_BlockDecrypt(block, dec, &ctx);

        // we need now to unxor this thing and push to result
        for(size_t i = 0; i < AES_BLOCK_SIZE_IN_BYTES; i++){
            dec[i] ^= ci1[i];
            out.push_back(dec[i]);
        }

        // update the xor block and move head
        ci1 = &input[head];
        head += AES_BLOCK_SIZE_IN_BYTES;
    }

    // check PKCS#7 padding
    uint8_t lastByte = dec[AES_BLOCK_SIZE_IN_BYTES-1];
    if(lastByte < 0x01 || lastByte > 0x10){
        printf("Invalid padding scheme ( %x )\n", lastByte);
        out.clear();
        return false;
    }

    for(size_t i = out.size()-1; i >= out.size()-lastByte; i--){
        if(out[i] != lastByte){
            printf("Invalid byte on padding\n");
            out.clear();
            return false;
        }
    }

    out.resize(out.size() - lastByte);

    return true;
}

bool AES_CBC_Encrypt(uint8_t *input, size_t len, uint8_t *key, AesKeyLength length,
                     std::vector<uint8_t> &out, std::vector<uint8_t> &giv)
{
    AesContext ctx;
    uint8_t block[AES_BLOCK_SIZE_IN_BYTES];
    uint8_t enc[AES_BLOCK_SIZE_IN_BYTES];
    uint8_t iv[AES_BLOCK_SIZE_IN_BYTES];
    uint8_t padByte = 0x10;
    size_t head = 0;
    bool is_last_block = false;
    uint8_t *ci1 = &iv[0];

    AES_InitializeContext(&ctx, length);
    if(ctx.keySize == 0){
        return false;
    }

    padByte = AES_BLOCK_SIZE_IN_BYTES - len % AES_BLOCK_SIZE_IN_BYTES;

    if(giv.size() != AES_BLOCK_SIZE_IN_BYTES){
        if(!Crypto_SecureRNG(iv, AES_BLOCK_SIZE_IN_BYTES)) return false;
    }else{
        for(size_t i = 0; i < AES_BLOCK_SIZE_IN_BYTES; i++){
            iv[i] = giv[i];
        }

        giv.clear();
    }

    AES_KeyExpansion(&ctx, (uint8_t *)key);

    while(!(is_last_block || len == 0)){
        size_t toCipher = AES_BLOCK_SIZE_IN_BYTES;
        // check if message is not multiple of block size
        if(toCipher > len){
            toCipher = len;
            is_last_block = true;
            len = 0;
        }else{
            len -= AES_BLOCK_SIZE_IN_BYTES;
        }

        memcpy(block, &input[head], toCipher);
        head += toCipher;

        if(is_last_block){
            // in case the message is not multiple add PKCS#7 padding
            for(size_t i = toCipher; i < AES_BLOCK_SIZE_IN_BYTES; i++){
                block[i] = padByte;
            }
        }

        // xor the block with the previous cipher
        for(size_t i = 0; i < AES_BLOCK_SIZE_IN_BYTES; i++){
            block[i] ^= ci1[i];
        }

        // encrypt the block
        AES_BlockEncrypt(block, enc, &ctx);

        // update the xor buffer and copy to output
        ci1 = &enc[0];
        for(size_t i = 0; i < AES_BLOCK_SIZE_IN_BYTES; i++){
            out.push_back(enc[i]);
        }
    }

    // if we finished because the message was multiple of the block size
    // we need to do one more to insert the padding
    if(!is_last_block){
        for(size_t i = 0; i < AES_BLOCK_SIZE_IN_BYTES; i++) block[i] = padByte;

        // xor the block with the previous cipher
        for(size_t i = 0; i < AES_BLOCK_SIZE_IN_BYTES; i++){
            block[i] ^= ci1[i];
        }

        // encrypt the block
        AES_BlockEncrypt(block, enc, &ctx);

        // push result
        for(size_t i = 0; i < AES_BLOCK_SIZE_IN_BYTES; i++){
            out.push_back(enc[i]);
        }
    }

    for(size_t i = 0; i < AES_BLOCK_SIZE_IN_BYTES; i++){
        giv.push_back(iv[i]);
    }

    return true;
}

#if defined(AES_TEST)

typedef struct NistTestEntry{
    std::string plaintext;
    std::string key;
    std::string cipher;
    std::string iv;
    AesKeyLength aes;
}NistTestEntry;

std::vector<NistTestEntry> nistVectors = {
    {"6bc1bee22e409f96e93d7e117393172a", "2b7e151628aed2a6abf7158809cf4f3c",
     "7649abac8119b246cee98e9b12e9197d", "000102030405060708090a0b0c0d0e0f", AES128},

    {"ae2d8a571e03ac9c9eb76fac45af8e51", "2b7e151628aed2a6abf7158809cf4f3c",
     "5086cb9b507219ee95db113a917678b2", "7649ABAC8119B246CEE98E9B12E9197D", AES128},

    {"30c81c46a35ce411e5fbc1191a0a52ef", "2b7e151628aed2a6abf7158809cf4f3c",
     "73bed6b8e3c1743b7116e69e22229516", "5086CB9B507219EE95DB113A917678B2", AES128},

    {"f69f2445df4f9b17ad2b417be66c3710", "2b7e151628aed2a6abf7158809cf4f3c",
     "3ff1caa1681fac09120eca307586e1a7", "73BED6B8E3C1743B7116E69E22229516", AES128},

    {"6bc1bee22e409f96e93d7e117393172a", "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
     "4f021db243bc633d7178183a9fa071e8", "000102030405060708090A0B0C0D0E0F", AES192},

    {"30c81c46a35ce411e5fbc1191a0a52ef", "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
     "571b242012fb7ae07fa9baac3df102e0", "B4D9ADA9AD7DEDF4E5E738763F69145A", AES192},

    {"f69f2445df4f9b17ad2b417be66c3710", "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
     "08b0e27988598881d920a9e64f5615cd", "571B242012FB7AE07FA9BAAC3DF102E0", AES192},

    {"6bc1bee22e409f96e93d7e117393172a", "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
     "f58c4c04d6e5f1ba779eabfb5f7bfbd6", "000102030405060708090A0B0C0D0E0F", AES256},

    {"30c81c46a35ce411e5fbc1191a0a52ef", "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
     "39f23369a9d9bacfa530e26304231461", "9CFC4E967EDB808D679F777BC6702C7D", AES256},

    {"f69f2445df4f9b17ad2b417be66c3710", "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
     "b2eb05e2c39be9fcda6c19078c6a9d1b", "39F23369A9D9BACFA530E26304231461", AES256},
};

void AES_RunVectorTests(){
    int c = 1;
    for(NistTestEntry &e : nistVectors){
        std::vector<unsigned char> text, iv, key, cipher, dec, enc;
        CryptoUtil_BufferFromHex(text, (char *)e.plaintext.c_str(), e.plaintext.size());
        CryptoUtil_BufferFromHex(iv, (char *)e.iv.c_str(), e.iv.size());
        CryptoUtil_BufferFromHex(key, (char *)e.key.c_str(), e.key.size());
        CryptoUtil_BufferFromHex(cipher, (char *)e.cipher.c_str(), e.cipher.size());

        printf(" ==== TEST %d ==== \n", c++);
        printf("Text:   %s\n", e.plaintext.c_str());
        printf("Key:    %s\n", e.key.c_str());
        printf("Iv:     %s\n", e.iv.c_str());
        printf("Cipher: %s\n", e.cipher.c_str());

        if(!AES_CBC_Encrypt(text.data(), text.size(), key.data(), e.aes, enc, iv)){
            printf(" ** FAILED\n");
            exit(0);
        }else{
            std::string res;
            CryptoUtil_BufferToHex(enc.data(), enc.size(), res);
            printf(" > Encryption : %s\n", res.c_str());
            // Nist test vectors do not apply PKCS#7 padding
            // so we need to remove it
            res = res.substr(0, res.size() / 2);
            if(res != e.cipher){
                printf(" ** ENCRYPTION FAILED\n");
                exit(0);
            }
        }

        if(!AES_CBC_Decrypt(enc.data(), enc.size(), key.data(), iv.data(), e.aes, dec)){
            printf(" ** FAILED\n");
            exit(0);
        }else{
            std::string res;
            CryptoUtil_BufferToHex(dec.data(), dec.size(), res);
            printf(" > Decryption : %s\n", res.c_str());
            if(res != e.plaintext){
                printf(" ** DECRYPTION FAILED\n");
                exit(0);
            }
        }
        printf(" > OK\n");
        printf("===================\n");
    }

    printf("Passed all %d tests\n", c-1);
}

#else
void AES_RunVectorTests(){
    printf("Cody is not compiled with NIST test vectors\n");
}
#endif
