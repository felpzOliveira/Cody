#include <security.h>
#include <types.h>
#include <aes.h>
#include <rng.h>
#include <cryptoutil.h>
#define LOG_MODULE "Security"
#include <log.h>

// TODO: This whole file is for testing and does not represent anything yet
// TODO: Key exchange with Frodo

// TODO: Remove this temporary key
uint8_t key[32] = { 0x19, 0x57, 0x72, 0x14, 0xf9, 0x50, 0xfd, 0x01, 0x95,
                    0x9b, 0x24, 0x4a, 0x0a, 0x05, 0x06, 0xcd, 0x5e, 0xb9,
                    0x8c, 0xc1, 0x28, 0xa4, 0x9d, 0x47, 0x02, 0xc4, 0x2a,
                    0x88, 0x0e, 0xb8, 0x65, 0xc5 };

void SecurityServices::CreateContext(SecurityServices::Context &context, uint8_t *userKey){
    if(userKey){
        memcpy(context.key, userKey, 32 * sizeof(uint8_t));
    }else{
        memcpy(context.key, key, 32 * sizeof(uint8_t));
    }

    context.is_initialized = true;
}

void SecurityServices::CopyContext(SecurityServices::Context *dst,
                                   SecurityServices::Context *src)
{
    memcpy(dst->key, src->key, 32 * sizeof(uint8_t));
    dst->is_initialized = src->is_initialized;
}

bool SecurityServices::Encrypt(Context *ctx, uint8_t *input, size_t len,
                               std::vector<uint8_t> &out)
{
    if(!ctx){
        LOG_ERR("No crypto context active");
        return false;
    }

    if(!ctx->is_initialized){
        LOG_ERR("Crypto context is not initialized during encrypt");
        return false;
    }

    std::vector<uint8_t> iv;
    bool rv = AES_CBC_Encrypt(input, len, (uint8_t *)ctx->key, AES256, out, iv);
    if(rv){
        for(int i = 0; i < AES_BLOCK_SIZE_IN_BYTES; i++){
            out.push_back(iv[i]);
        }
    }
    return rv;
}

bool SecurityServices::Decrypt(Context *ctx, uint8_t *input, size_t len,
                               std::vector<uint8_t> &out)
{
    if(!ctx){
        LOG_ERR("No crypto context active");
        return false;
    }

    if(!ctx->is_initialized){
        LOG_ERR("Crypto context is not initialized during decrypt");
        return false;
    }

    uint8_t *iv = nullptr;
    uint8_t *enc = input;

    if(len <= AES_BLOCK_SIZE_IN_BYTES) return false;
    if(!input) return false;

    iv = &input[len - AES_BLOCK_SIZE_IN_BYTES];
    len -= AES_BLOCK_SIZE_IN_BYTES;

    return AES_CBC_Decrypt(enc, len, (uint8_t *)ctx->key, iv, AES256, out);
}

bool SecurityServices::CreateChallenge(Context *ctx, std::vector<uint8_t> &out,
                                       Challenge &ch)
{
    bool rv = Crypto_SecureRNG((void *)&ch, ChallengeSizeInBytes);
    if(!rv) return false;
    rv = SecurityServices::Encrypt(ctx, (uint8_t *)&ch, ChallengeSizeInBytes, out);
    return rv;
}

bool SecurityServices::SolveChallenge(Context *ctx, uint8_t *input, size_t len,
                                      std::vector<uint8_t> &out)
{
    Challenge value = 0;
    std::vector<uint8_t> dec;
    bool rv = SecurityServices::Decrypt(ctx, input, len, dec);
    if(!rv) return false;
    if(dec.size() != ChallengeSizeInBytes){
        printf("Decrypted size is not %d (%d)\n", ChallengeSizeInBytes, (int)dec.size());
        return false;
    }

    memcpy(&value, dec.data(), ChallengeSizeInBytes);
    value += 1;
    return SecurityServices::Encrypt(ctx, (uint8_t *)&value, ChallengeSizeInBytes, out);
}

bool SecurityServices::IsChallengeSolved(Context *ctx, uint8_t *input,
                                         size_t len, Challenge &ch)
{
    uint64_t value = 0;
    std::vector<uint8_t> dec;
    bool rv = SecurityServices::Decrypt(ctx, input, len, dec);
    if(!rv) return false;
    if(dec.size() != ChallengeSizeInBytes){
        printf("Decrypted size is not %d (%d)\n", ChallengeSizeInBytes, (int)dec.size());
        return false;
    }

    memcpy(&value, dec.data(), ChallengeSizeInBytes);
    return (value == ch + 1);
}

unsigned int SecurityServices::PackageRequiredSize(unsigned int size){
    return AES_CBC_ComputeRequiredLength(size) + AES_BLOCK_SIZE_IN_BYTES;
}

unsigned int SecurityServices::ChallengeExpectedSize(){
    return PackageRequiredSize(ChallengeSizeInBytes);
}

