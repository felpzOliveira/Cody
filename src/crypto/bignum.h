/* date = June 10th 2021 11:30 */
#if !defined(BIG_NUM_H)
#define BIG_NUM_H

#include <types.h>

typedef struct Bignum{
    ullong *arr;
    int top;
    int amax;
    int neg;
    int flags;
}Bignum;

#endif
