/* date = May 25th 2022 19:31 */
#pragma once

#include <types.h>
#include <vector>

#define Challenge uint64_t
#define ChallengeSizeInBytes 8

namespace SecurityServices{

    struct Context{
        bool is_initialized;
        uint8_t key[32];

        Context() : is_initialized(false){}
    };

    /*
     * Contexts for encryption/decryption operations.
     */
    void CreateContext(Context &context, uint8_t *key=nullptr);

    /*
     * Copy a context.
     */
    void CopyContext(Context *dst, Context *src);

    /*
     * Wrappers for AES-256 that contains both the encrypted/decrypted data
     * and the iv used (for encryption only). Use this as send/recv interface.
     */
    bool Encrypt(Context *ctx, uint8_t *input, size_t len, std::vector<uint8_t> &out);
    bool Decrypt(Context *ctx, uint8_t *input, size_t len, std::vector<uint8_t> &out);

    /*
     * Challenge-Response related operations.
     */
    bool CreateChallenge(Context *ctx, std::vector<uint8_t> &out, Challenge &ch);
    bool SolveChallenge(Context *ctx, uint8_t *input, size_t len, std::vector<uint8_t> &out);
    bool IsChallengeSolved(Context *ctx, uint8_t *input, size_t len, Challenge &ch);

    /*
     * Returns the size of the package generated for the challenge and the size
     * of the proper response which is 32 bytes = 2 * AES_BLOCK_SIZE_IN_BYTES.
     * One for the encrypted value itself and other for the encryption iv.
     */
    unsigned int ChallengeExpectedSize();

    /*
     * Returns the amount of bytes required for encryption of 'size' bytes.
     */
    unsigned int PackageRequiredSize(unsigned int size);
}

