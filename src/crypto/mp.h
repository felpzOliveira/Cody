/* date = October 28th 2021 19:37 */
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <string>

/*
 * Multiple Precision Integers for public key based cryptography.
 * This implementation is based on the book:
 *   Bignum Math: Implementing Cryptographic Multiple Precision Arithmetic.
 * Book is quite nice and I think MbedTLS is based on it, looks pretty close.
 */

#define MP_ZPOS          0
#define MP_NEG           1
#define MP_DEFAULT_SIZE 32
#define MP_DIGIT_BIT    60
#define MP_MAX_DIGIT_COUNT ((INT_MAX - 2) / MP_DIGIT_BIT)
#define MP_MASK ((((MP_Digit)1)<<((MP_Digit)MP_DIGIT_BIT))-((MP_Digit)1))
#define MP_IS_2EXPT(x) (((x) != 0u) && (((x) & ((x) - 1u)) == 0u))

typedef enum{
    MP_GT, MP_LT, MP_EQ,
}MPCmp;

typedef uint64_t MP_Digit;

typedef struct MP_Int{
    int used;
    int alloc;
    int sign;
    MP_Digit *digits;
}MP_Int;

/*
 * Initializes a MP_Int structure and set it to 0.
 */
bool MP_Init(MP_Int *mp);

/*
 * Initializes a MP_Int 'a' as a copy of another MP_Int 'mp'.
 */
bool MP_InitCopy(MP_Int *a, MP_Int *mp);

#define MP_InitMulti(...) _MP_InitMulti(__VA_ARGS__, NULL)
bool _MP_InitMulti(MP_Int *mp, ...);

/*
 * Initializes a MP_Int with a specific size.
 */
bool MP_InitSize(MP_Int *mp, int size);

/*
 * Initializes a MP_Int from a string in base radix.
 */
bool MP_ReadRadix(MP_Int *a, const char *str, int radix);

/*
 * Releases the contents of a MP_Int.
 */
void MP_Clear(MP_Int *mp);

/*
 * Make a MP_Int become 0.
 */
void MP_Zero(MP_Int *mp);

/*
 * Copies a MP_Int into another: b = a.
 */
bool MP_Copy(MP_Int *a, MP_Int *b);

/*
 * Computes the absolute value of an MP_Int: b = |a|.
 */
bool MP_Abs(MP_Int *a, MP_Int *b);

/*
 * Negates a MP_Int: b = -a.
 */
bool MP_Neg(MP_Int *a, MP_Int *b);

/*
 * Sets a MP_Int to ONE digit d, better for small assignments.
 */
void MP_Set(MP_Int *mp, MP_Digit d);

/* specific sets */
void MP_SetU32(MP_Int *mp, uint32_t b);
void MP_SetU64(MP_Int *mp, uint64_t b);
void MP_SetI32(MP_Int *mp, int32_t b);
void MP_SetI64(MP_Int *mp, int64_t b);

/*
 * Compares two MP_Int, returns:
 *   - MP_GT => a > b
 *   - MP_LT => a < b
 *   - MP_EQ => a == b
 */
MPCmp MP_CmpMag(MP_Int *a, MP_Int *b);
MPCmp MP_Cmp(MP_Int *a, MP_Int *b);

/*
 * Computes the signed result of adding two MP_Int: c = a + b.
 */
bool MP_Add(MP_Int *a, MP_Int *b, MP_Int *c);

/*
 * Computes the signed result of subtracting two MP_Int: c = a - b.
 */
bool MP_Sub(MP_Int *a, MP_Int *b, MP_Int *c);

/*
 * Computes the amount of bits in an MP_Int.
 */
size_t MP_BitLength(MP_Int *mp);

/*
 * Prints a MP_Int into radix format.
 */
bool MP_ToRadix(MP_Int *a, std::string &str, int radix);

/*
 * High level container for easier math operations.
 */
class MPI{
    public:
    MP_Int val;

    MPI(){ MP_Init(&val); }
    MPI(const MPI &mp){ MP_InitCopy(&val, (MP_Int *)&mp.val); }

    MPI &operator=(const MPI &mp){
        if(!MP_Copy((MP_Int *)&mp.val, &val)){
            printf("MP error failed copy\n");
        }
        return *this;
    }

    MPI operator+(const MPI &mp){
        MPI mpi;
        if(!MP_Add(&val, (MP_Int *)&mp.val, &mpi.val)){
            printf("MP error on operator+\n");
        }
        return mpi;
    }

    MPI &operator+=(const MPI &mp){
        if(!MP_Add(&val, (MP_Int *)&mp.val, &val)){
            printf("MP error on operator+=\n");
        }
        return *this;
    }

    MPI operator-(const MPI &mp){
        MPI mpi;
        if(!MP_Sub(&val, (MP_Int *)&mp.val, &mpi.val)){
            printf("MP error on operator-\n");
        }

        return mpi;
    }

    MPI &operator-=(const MPI &mp){
        if(!MP_Sub(&val, (MP_Int *)&mp.val, &val)){
            printf("MP error on operator+=\n");
        }
        return *this;
    }

    int Sign(){
        if(val.used == 0) return 0;
        if(val.sign == MP_NEG) return -1;
        return 1;
    }

    std::string ToRadix(int radix){
        std::string str;
        MP_ToRadix(&val, str, radix);
        return str;
    }

    std::string ToHex(){ return ToRadix(16); }

    ~MPI(){ MP_Clear(&val); }

    static MPI FromU32(uint32_t e){
        MPI mpi;
        MP_SetU32(&mpi.val, e);
        return mpi;
    }

    static MPI FromU64(uint64_t e){
        MPI mpi;
        MP_SetU64(&mpi.val, e);
        return mpi;
    }

    static MPI FromI32(int32_t e){
        MPI mpi;
        MP_SetI32(&mpi.val, e);
        return mpi;
    }

    static MPI FromI64(int64_t e){
        MPI mpi;
        MP_SetI64(&mpi.val, e);
        return mpi;
    }

    static MPI FromRadix(std::string str, int radix){
        MPI mpi;
        MP_ReadRadix(&mpi.val, str.c_str(), radix);
        return mpi;
    }

    static MPI FromHex(std::string hex){
        return FromRadix(hex, 16);
    }
};
