/* date = October 25th 2021 22:15 */
#pragma once

/*
 * Disclaimer: All crypto code present in Cody is meant to be both
 * learn material for myself and provide a sandbox for basic operations.
 * This means that while I try my best to make this code safe it is meant
 * to protect against connection stuff and not local stuff, i.e.: I won't
 * bother with things like: caching attack and other side channels attacks
 * as for this application it makes no sense to worry about that and it would
 * only make it harder to implement these algorithms. I'm a strong believer
 * on 2 things:
 *   1 - We should consider implementing things ourselves, it's the only way
 *       we learn and maybe get things to a better place;
 *   2 - Applications should use what they need and not fantasize about
 *       things that are impossible in their scenarios.
 * This implies that my implementation is meant to provide protection on network
 * connections and not on local machines, If you feel like this code should
 * do more I suggest you implement crypto using either OpenSSL or MbedTLS.
 * I also don't make any promises that this will be decently fast, performance
 * is also irrelevant for Cody. I'll eventually tabulate things but only when
 * actually required.
 */

typedef enum{
    AES128,
    AES192,
    AES256
}AesKeyLength;

/*
 * Generates a AES key with a given size, basically a random array.
 */
bool AES_GenerateKey(void *buffer, AesKeyLength length);
