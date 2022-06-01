/* date = May 25th 2022 19:31 */
#pragma once

#include <types.h>
#include <vector>

#define Challenge uint64_t
#define ChallengeSizeInBytes 8

namespace SecurityServices{
    /*
     * Wrappers for AES-256 that contains both the encrypted/decrypted data
     * and the iv used (for encryption only). Use this when send/recv messages.
     */
    bool Encrypt(uint8_t *input, size_t len, std::vector<uint8_t> &out);
    bool Decrypt(uint8_t *input, size_t len, std::vector<uint8_t> &out);

    /*
     * We are going to use a challenge-response authentication design, however
     * currently we don't support key-exchange with Frodo yet as asymetric implementation
     * is missing. So currently we simply test if the key is the same with a simple AES
     * encryption. The code has a hardcoded key for testing purposes but if you are using
     * this and Frodo is not yet implemented you can overwrite this key with a given key
     * bellow and use the flag --key when launching.
     */
    void Start(uint8_t *userKey=nullptr);

    bool CreateChallenge(std::vector<uint8_t> &out, Challenge &ch);
    bool SolveChallenge(uint8_t *input, size_t len, std::vector<uint8_t> &out);
    bool IsChallengeSolved(uint8_t *input, size_t len, Challenge &ch);

    /*
     * Returns the size of the package generated for the challenge and the size
     * of the proper response which is 32 bytes = 2 * AES_BLOCK_SIZE_IN_BYTES.
     * One for the encrypted value itself and other for the encryption iv.
     */
    unsigned int ChallengeExpectedSize();

    unsigned int PackageRequiredSize(unsigned int size);
}

