#include <hash.h>

#define FNV1_32_INIT ((uint)0x811c9dc5)
#define FNV1_64_INIT ((uint64)0xcbf29ce484222325ULL)
#define FNV_32_PRIME ((uint)0x01000193)
#define FNV_64_PRIME ((uint64)0x100000001b3ULL)

uint64 FowlerNollVoStringHash64(char *str){
    uint64 fnvHval = FNV1_64_INIT;
    unsigned char *s = (unsigned char *)str;
    while(*s){
        // perform xor with octect
        fnvHval ^= (uint64) *s++;

        // fast prime mod 2^64
#if 1
        fnvHval *= FNV_64_PRIME;
#else
        fnvHval = ((fnvHval << 1) + (fnvHval << 4) + (fnvHval << 5)
                   (fnvHval << 7) + (fnvHval << 8) + (fnvHval << 40));
#endif
    }

    return fnvHval;
}

uint FowlerNollVoStringHash32(char *str){
    uint fnvHval = FNV1_32_INIT;
    unsigned char *s = (unsigned char *)str;
    while(*s){
        // perform xor with octect
        fnvHval ^= (uint) *s++;

        // fast prime mod 2^32
#if 1
        fnvHval *= FNV_32_PRIME;
#else
        fnvHval = ((fnvHval << 1) + (fnvHval << 4) +
                   (fnvHval << 7) + (fnvHval << 8) + (fnvHval << 24));
#endif
    }

    return fnvHval;
}

uint MurmurHash3(char *str, int len, uint seed){
    const unsigned char * data = (const unsigned char*)str;
    const int nblocks = len / 4;

    uint h1 = seed;

    const uint c1 = 0xcc9e2d51;
    const uint c2 = 0x1b873593;
    const uint * blocks = (const uint *)(data + nblocks * 4);

    for(int i = -nblocks; i; i++){
        uint k1 = blocks[i];

        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;

        h1 ^= k1;
        h1 = (h1 << 15) | (h1 >> 19);
        h1 = h1 * 5 + 0xe6546b64;
    }

    const unsigned char * tail = (const unsigned char*)(data + nblocks * 4);
    uint k1 = 0;

    switch(len & 3){
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        h1 ^= k1;
    };

    h1 ^= len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;
    return h1;
}

uint MurmurHash2(char *str, int len, uint seed){
    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.

    const uint m = 0x5bd1e995;
    const int r = 24;

    // Initialize the hash to a 'random' value
    uint h = seed ^ len;

    // Mix 4 bytes at a time into the hash
    const unsigned char * data = (const unsigned char *)str;

    while(len >= 4){
        uint k = *(uint *)data;
        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    // Handle the last few bytes of the input array
    switch(len){
        case 3: h ^= data[2] << 16;
        case 2: h ^= data[1] << 8;
        case 1: h ^= data[0];
        h *= m;
    };

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}
