#include <mp.h>
#include <types.h>
#include <stddef.h>
#include <stdarg.h>
#include <limits.h>
#include <algorithm>

#define MP_MAX(a, b) ((a) > (b) ? (a) : (b))
#define MP_SIZEOF_BITS(type) ((size_t)CHAR_BIT * sizeof(type))
#define MP_MIN_DIGIT_COUNT MP_MAX(3, (((int)MP_SIZEOF_BITS(uint64_t) + MP_DIGIT_BIT) - 1) / MP_DIGIT_BIT)
#define MP_IsZero(a) ((a)->used == 0)
#define MP_IsNeg(a)  ((a)->sign == MP_NEG)
#define MP_IsEven(a) (((a)->used == 0) || (((a)->digits[0] & 1u) == 0u))
#define MP_IsOdd(a)  (!MP_IsEven(a))
#define MP_EXCH(t, a, b) do{ t _c = a; a = b; b = _c; } while (0)
#define MP_TOUPPER(c) ((((c) >= 'a') && ((c) <= 'z')) ? (((c) + 'A') - 'a') : (c))
#define MP_RADIX_MAP_REVERSE_SIZE 80u
#define MP_DIGIT_MAX     MP_MASK

const char s_mp_radix_map[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+/";
const uint8_t s_mp_radix_map_reverse[] = {
    0x3e, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x01, 0x02, 0x03, 0x04, /* +,-./01234 */
    0x05, 0x06, 0x07, 0x08, 0x09, 0xff, 0xff, 0xff, 0xff, 0xff, /* 56789:;<=> */
    0xff, 0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, /* ?@ABCDEFGH */
    0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, /* IJKLMNOPQR */
    0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0xff, 0xff, /* STUVWXYZ[\ */
    0xff, 0xff, 0xff, 0xff, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, /* ]^_`abcdef */
    0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, /* ghijklmnop */
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d  /* qrstuvwxyz */
};

typedef unsigned long mp_word __attribute__((mode(TI)));

void MP_ZeroDigits(MP_Digit *d, int digits){
    while(digits-- > 0){
        *d++ = 0;
    }
}

void MP_Exch(MP_Int *a, MP_Int *b){
    MP_EXCH(MP_Int, *a, *b);
}

/***************************************************************/
//    Support Routines - Init/Clear/Sets/Copies/...            //
/***************************************************************/
void MP_Clamp(MP_Int *mp){
    while((mp->used > 0) && (mp->digits[mp->used-1] == 0u)){
        mp->used -= 1;
    }

    if(MP_IsZero(mp)){
        mp->sign = MP_ZPOS;
    }
}

void MP_Zero(MP_Int *mp){
    mp->used = 0;
    mp->sign = MP_ZPOS;
    for(int i = 0; i < mp->alloc; i++){
        mp->digits[i] = 0;
    }
}

void MP_Clear(MP_Int *mp){
    if(mp->digits){
        for(int i = 0; i < mp->alloc; i++){
            mp->digits[i] = 0;
        }

        AllocatorFree(mp->digits);
        mp->digits = nullptr;
        mp->alloc = 0;
        mp->used = 0;
        mp->sign = MP_ZPOS;
    }
}

bool MP_Neg(MP_Int *a, MP_Int *b){
    if(!MP_Copy(a, b)) return false;
    b->sign = (!MP_IsZero(b) && !MP_IsNeg(b)) ? MP_NEG : MP_ZPOS;
    return true;
}

bool MP_Grow(MP_Int *mp, int size){
    if(mp->alloc < size){
        MP_Digit *digit;
        if(size > MP_MAX_DIGIT_COUNT){
            printf("Unsupported, max digit count is %d\n", MP_MAX_DIGIT_COUNT);
            return false;
        }

        digit = AllocatorExpand(MP_Digit, mp->digits, mp->alloc, size);
        if(!digit){
            printf("Failed realloc\n");
            return false;
        }

        mp->digits = digit;
        for(int i = mp->alloc; i < size; i++){
            mp->digits[i] = 0;
        }

        mp->alloc = size;
    }

    return true;
}

bool MP_Copy(MP_Int *a, MP_Int *b){
    if(a == b) return true;

    bool rv = MP_Grow(b, a->used);
    if(!rv) return false;

    for(int i = 0; i < a->used; i++){
        b->digits[i] = a->digits[i];
    }

    for(int i = 0; i < b->used - a->used; i++){
        b->digits[i + a->used] = 0;
    }

    b->used = a->used;
    b->sign = a->sign;
    return true;
}

bool MP_InitSize(MP_Int *mp, int size){
    size = MP_MAX(size, MP_MIN_DIGIT_COUNT);
    if(size > MP_MAX_DIGIT_COUNT){
        printf("Unsupported, max digit count is %d\n", MP_MAX_DIGIT_COUNT);
        return false;
    }

    mp->digits = (MP_Digit *)AllocatorGet(sizeof(MP_Digit) * size);
    if(!mp->digits) return false;

    for(int i = 0; i < size; i++){
        mp->digits[i] = 0;
    }

    mp->sign = MP_ZPOS;
    mp->alloc = size;
    mp->used = 0;
    return true;
}

bool MP_Init(MP_Int *mp){
    mp->digits = (MP_Digit *)AllocatorGet(sizeof(MP_Digit) * MP_DEFAULT_SIZE);
    if(!mp->digits) return false;

    for(int i = 0; i < MP_DEFAULT_SIZE; i++){
        mp->digits[i] = 0;
    }

    mp->sign = MP_ZPOS;
    mp->alloc = MP_DEFAULT_SIZE;
    mp->used = 0;
    return true;
}

bool MP_Abs(MP_Int *a, MP_Int *b){
    bool rv = MP_Copy(a, b);
    if(!rv) return false;

    b->sign = MP_ZPOS;
    return rv;
}

bool MP_InitCopy(MP_Int *a, MP_Int *mp){
    bool rv = MP_InitSize(a, mp->used);
    if(!rv) return false;

    rv = MP_Copy(mp, a);
    if(!rv){
        MP_Clear(a);
    }

    return rv;
}

void MP_Set(MP_Int *mp, MP_Digit d){
    int oused = mp->used;
    mp->digits[0] = d & MP_MASK;
    mp->sign = MP_ZPOS;
    mp->used = (mp->digits[0] != 0u) ? 1 : 0;
    for(int i = mp->used; i < oused; i++){
        mp->digits[i] = 0;
    }
}

template<typename T>
void MP_SetUnsigned(MP_Int *mp, T b){
    int i = 0;
    while(b != 0u){
        mp->digits[i++] = ((MP_Digit)b & MP_MASK);
        if(MP_SIZEOF_BITS(T) <= MP_DIGIT_BIT) break;
        b >>= (MP_SIZEOF_BITS(T) <= MP_DIGIT_BIT) ? 0 : MP_DIGIT_BIT;
    }

    mp->used = i;
    mp->sign = MP_ZPOS;
    MP_ZeroDigits(mp->digits + mp->used, mp->alloc - mp->used);
}

void MP_SetU32(MP_Int *mp, uint32_t b){
    MP_SetUnsigned(mp, b);
}

void MP_SetU64(MP_Int *mp, uint64_t b){
    MP_SetUnsigned(mp, b);
}

void MP_SetI32(MP_Int *mp, int32_t b){
    MP_SetU32(mp, b < 0 ? -b : b);
    if(b < 0) mp->sign = MP_NEG;
}

void MP_SetI64(MP_Int *mp, int64_t b){
    MP_SetU64(mp, b < 0 ? -b : b);
    if(b < 0) mp->sign = MP_NEG;
}

MPCmp MP_CmpMag(MP_Int *a, MP_Int *b){
    if(a->used != b->used){
        return a->used > b->used ? MP_GT : MP_LT;
    }

    for(int i = a->used; i-- > 0;){
        if(a->digits[i] != b->digits[i]){
            return a->digits[i] > b->digits[i] ? MP_GT : MP_LT;
        }
    }

    return MP_EQ;
}

MPCmp MP_Cmp(MP_Int *a, MP_Int *b){
    if(a->sign != b->sign){
        return MP_IsNeg(a) ? MP_LT : MP_GT;
    }

    if(MP_IsNeg(a)){
        return MP_CmpMag(b, a);
    }else{
        return MP_CmpMag(a, b);
    }
}

bool _MP_InitMulti(MP_Int *mp, ...){
    bool rv = true;
    int n = 0;
    MP_Int *curr = mp;
    va_list args;

    va_start(args, mp);
    while(curr != NULL){
        rv = MP_Init(curr);
        if(!rv){
            va_list clean_arg;
            curr = mp;
            va_start(clean_arg, mp);
            while(n-- != 0){
                MP_Clear(curr);
                curr = va_arg(clean_arg, MP_Int *);
            }

            va_end(clean_arg);
            break;
        }

        n++;
        curr = va_arg(args, MP_Int *);
    }

    va_end(args);

    return rv;
}

size_t MP_BitLength(MP_Int *mp){
    size_t i = 0, clz = 0, j = 0;
    size_t c = sizeof(MP_Digit);
    size_t b = c << 3;

    if(mp->used == 0) return 0;
    for(i = mp->used-1; i > 0; i--){
        if(mp->digits[i] != 0){
            break;
        }
    }

    MP_Digit mask = (MP_Digit)1 << (b - 1);
    for(clz = 0; clz < b; clz++){
        if(mp->digits[i] & mask) break;
        mask >>= 1;
    }

    j = b - clz;
    return i * b + j;
}

/***************************************************************/
//                     Basic Arithmetic                        //
/***************************************************************/
bool MP_AddD(MP_Int *a, MP_Digit b, MP_Int *c);
bool MP_SubD(MP_Int *a, MP_Digit b, MP_Int *c);

/* Must satisfy |a| > |b| */
static bool MP_Sub_s(MP_Int *mp0, MP_Int *mp1, MP_Int *c){
    int oused = c->used;
    int i = 0;
    MP_Digit carry = 0;
    MP_Int *a = mp0;
    MP_Int *b = mp1;
    int min = b->used;
    int max = a->used;

    if(!MP_Grow(c, max)) return false;

    c->used = max;
    for(i = 0; i < min; i++){
        // Computes C[i] = A[i] - B[i] - Carry
        c->digits[i] = (a->digits[i] - b->digits[i]) - carry;

        // grab the carry from C[i]
        carry = c->digits[i] >> (MP_SIZEOF_BITS(MP_Digit) - 1u);

        // remove the carry
        c->digits[i] &= MP_MASK;
    }

    // copy the rest of the digits
    for(; i < max; i++){
        // remove carry
        c->digits[i] = a->digits[i] - carry;

        // just like before propagate the carry
        carry = c->digits[i] >> (MP_SIZEOF_BITS(MP_Digit) - 1u);
        c->digits[i] &= MP_MASK;
    }

    MP_ZeroDigits(c->digits + c->used, oused - c->used);
    MP_Clamp(c);
    return true;
}

/* Unsigned sum c = |a| + |b| */
static bool MP_Add_s(MP_Int *mp0, MP_Int *mp1, MP_Int *c){
    int oused = 0, min = 0, max = 0, i = 0;
    MP_Digit carry = 0;
    MP_Int *a = mp0;
    MP_Int *b = mp1;

    if(a->used < b->used){
        a = mp1; b = mp0;
    }

    min = b->used;
    max = a->used;

    if(!MP_Grow(c, max+1)) return false;

    oused = c->used;
    c->used = max+1;

    /*
    * The sum is **literally** what we would do on paper, for each
    * digit sum them and compute the carry.
    */
    for(i = 0; i < min; i++){
        // compute C[i] = A[i] + B[i] + Carry
        c->digits[i] = a->digits[i] + b->digits[i] + carry;

        // grab the carry from C[i]
        carry = c->digits[i] >> (MP_Digit)MP_DIGIT_BIT;

        // remove the carry
        c->digits[i] &= MP_MASK;
    }

    /*
    * Now copy the remaining digits from the largest number
    */
    if(min != max){
        for(; i < max; i++){
            // add carry to C[i] = A[i] + Carry
            c->digits[i] = a->digits[i] + carry;

            // just like before propagate the carry
            carry = c->digits[i] >> (MP_Digit)MP_DIGIT_BIT;
            c->digits[i] &= MP_MASK;
        }
    }

    // the final position receives the carry
    c->digits[i] = carry;

    // clear the other digits
    MP_ZeroDigits(c->digits + c->used, oused - c->used);
    MP_Clamp(c);
    return true;
}

bool MP_Mul2(MP_Int *a, MP_Int *b){
    int x, oused;
    MP_Digit r;

    if(!MP_Grow(b, a->used+1)) return false;

    oused = b->used;
    b->used = a->used;

    r = 0;
    for(x = 0; x < a->used; x++){
        MP_Digit rr = a->digits[x] >> (MP_Digit)(MP_DIGIT_BIT-1);

        b->digits[x] = ((a->digits[x] << 1uL) | r) & MP_MASK;

        r = rr;
    }

    if(r != 0u){
        b->digits[b->used++] = 1;
    }

    // clear the other digits
    MP_ZeroDigits(b->digits + b->used, oused - b->used);
    b->sign = a->sign;

    return true;
}

bool MP_Div2(MP_Int *a, MP_Int *b){
    int x, oused;
    MP_Digit r;

    if(!MP_Grow(b, a->used)) return false;

    oused = b->used;
    b->used = a->used;

    r = 0;
    for(x = b->used; x-- > 0;){
        MP_Digit rr = a->digits[x] & 1u;

        b->digits[x] = (a->digits[x] >> 1uL) | (r << (MP_DIGIT_BIT-1));

        r = rr;
    }

    // clear the other digits
    MP_ZeroDigits(b->digits + b->used, oused - b->used);
    b->sign = a->sign;
    MP_Clamp(b);
    return true;
}

void MP_Rshd(MP_Int *a, int b){
    int x;
    if(b <= 0) return;

    if(b >= a->used){
        MP_Zero(a);
        return;
    }

    for(x = 0; x < (a->used-b); x++){
        a->digits[x] = a->digits[x+b];
    }

    MP_ZeroDigits(a->digits + a->used - b, b);
    a->used -= b;
}

bool MP_Lshd(MP_Int *a, int b){
    int x;
    if(b <= 0) return true;

    if(MP_IsZero(a)) return true;

    if(!MP_Grow(a, a->used + b)) return false;
    a->used += b;

    for(x = a->used; x -- > b;){
        a->digits[x] = a->digits[x-b];
    }

    MP_ZeroDigits(a->digits, b);
    return true;
}

bool MP_Mul2d(MP_Int *a, int b, MP_Int *c){
    if(b < 0){
        return false;
    }

    if(!MP_Copy(a, c)){
        return false;
    }

    if(!MP_Grow(c, c->used + (b / MP_DIGIT_BIT) + 1)){
        return false;
    }

    /* shift by as many digits in the bit count */
    if(b >= MP_DIGIT_BIT){
        if(!MP_Lshd(c, b / MP_DIGIT_BIT)){
            return false;
        }
    }

    /* shift any bit count < MP_DIGIT_BIT */
    b %= MP_DIGIT_BIT;
    if(b != 0u){
        MP_Digit shift, mask, r;
        int x;

       /* bitmask for carries */
        mask = ((MP_Digit)1 << b) - (MP_Digit)1;

       /* shift for msbs */
        shift = (MP_Digit)(MP_DIGIT_BIT - b);

       /* carry */
        r = 0;
        for(x = 0; x < c->used; x++){
            /* get the higher bits of the current word */
            MP_Digit rr = (c->digits[x] >> shift) & mask;

            /* shift the current word and OR in the carry */
            c->digits[x] = ((c->digits[x] << b) | r) & MP_MASK;

            /* set the carry to the carry bits of the current word */
            r = rr;
        }

        /* set final carry */
        if(r != 0u){
            c->digits[(c->used)++] = r;
        }
    }

    MP_Clamp(c);
    return true;
}

bool MP_Mod2d(MP_Int *a, int b, MP_Int *c){
    int x;
    if(b < 0){
        return false;
    }

    if(b == 0){
        MP_Zero(c);
        return true;
    }

    /* if the modulus is larger than the value than return */
    if(b >= (a->used * MP_DIGIT_BIT)){
        return MP_Copy(a, c);
    }

    if(!MP_Copy(a, c)){
        return false;
    }

    /* zero digits above the last digit of the modulus */
    x = (b / MP_DIGIT_BIT) + (((b % MP_DIGIT_BIT) == 0) ? 0 : 1);
    MP_ZeroDigits(c->digits + x, c->used - x);

    /* clear the digit that is not completely outside/inside the modulus */
    c->digits[b / MP_DIGIT_BIT] &= ((MP_Digit)1 << (MP_Digit)(b % MP_DIGIT_BIT)) - (MP_Digit)1;
    MP_Clamp(c);
    return true;
}

bool MP_Div2d(MP_Int *a, int b, MP_Int *c, MP_Int *d){
    if(b < 0){
        return false;
    }

    if(!MP_Copy(a, c)){
        return false;
    }

    /* 'a' should not be used after here - it might be the same as d */

    /* get the remainder */
    if(d != NULL) {
        if(!MP_Mod2d(a, b, d)){
            return false;
        }
    }

    /* shift by as many digits in the bit count */
    if(b >= MP_DIGIT_BIT){
        MP_Rshd(c, b / MP_DIGIT_BIT);
    }

    /* shift any bit count < MP_DIGIT_BIT */
    b %= MP_DIGIT_BIT;
    if(b != 0u){
        int x;
        MP_Digit r, mask, shift;

        /* mask */
        mask = ((MP_Digit)1 << b) - 1uL;

        /* shift for lsb */
        shift = (MP_Digit)(MP_DIGIT_BIT - b);

        /* carry */
        r = 0;
        for(x = c->used; x --> 0;){
            /* get the lower  bits of this word in a temp */
            MP_Digit rr = c->digits[x] & mask;

            /* shift the current word and mix in the carry bits from the previous word */
            c->digits[x] = (c->digits[x] >> b) | (r << shift);

            /* set the carry to the carry bits of the current word found above */
            r = rr;
        }
    }

    MP_Clamp(c);
    return true;
}

bool MP_MulD(MP_Int *a, MP_Digit b, MP_Int *c){
    MP_Digit u;
    bool   err;
    int   ix, oldused;

    if(b == 1u){
        return MP_Copy(a, c);
    }

    /* power of two ? */
    if((b == 2u)){
        return MP_Mul2(a, c);
    }
    if(MP_IS_2EXPT(b)){
        ix = 1;
        while ((ix < MP_DIGIT_BIT) && (b != (((MP_Digit)1)<<ix))) {
            ix++;
        }
        return MP_Mul2d(a, ix, c);
    }

    /* make sure c is big enough to hold a*b */
    if((err = MP_Grow(c, a->used + 1)) != true){
        return err;
    }

    /* get the original destinations used count */
    oldused = c->used;

    /* set the sign */
    c->sign = a->sign;

    /* zero carry */
    u = 0;

    /* compute columns */
    for(ix = 0; ix < a->used; ix++){
        /* compute product and carry sum for this term */
        mp_word r       = (mp_word)u + ((mp_word)a->digits[ix] * (mp_word)b);

        /* mask off higher bits to get a single digit */
        c->digits[ix] = (MP_Digit)(r & (mp_word)MP_MASK);

        /* send carry into next iteration */
        u       = (MP_Digit)(r >> (mp_word)MP_DIGIT_BIT);
    }

    /* store final carry [if any] and increment ix offset  */
    c->digits[ix] = u;

    /* set used count */
    c->used = a->used + 1;

    /* now zero digits above the top */
    MP_ZeroDigits(c->digits + c->used, oldused - c->used);

    MP_Clamp(c);

    return true;
}

bool MP_DivD(MP_Int *a, MP_Digit b, MP_Int *c, MP_Digit *d){
    int ix;
    bool err;
    /* cannot divide by zero */
    if(b == 0u){
        return false;
    }

    /* quick outs */
    if((b == 1u) || MP_IsZero(a)) {
        if(d != NULL){
            *d = 0;
        }
        if(c != NULL){
            return MP_Copy(a, c);
        }

        return true;
    }

    /* power of two ? */
    if((b == 2u)){
        if(d != NULL){
            *d = MP_IsOdd(a) ? 1u : 0u;
        }
        return (c == NULL) ? true : MP_Div2(a, c);
    }

    if(MP_IS_2EXPT(b)){
        ix = 1;
        while((ix < MP_DIGIT_BIT) && (b != (((MP_Digit)1)<<ix))){
            ix++;
        }
        if(d != NULL){
            *d = a->digits[0] & (((MP_Digit)1<<(MP_Digit)ix) - 1uL);
        }
        return (c == NULL) ? true : MP_Div2d(a, ix, c, NULL);
    }

    MP_Int  q;
    mp_word w;
    /* no easy answer [c'est la vie].  Just division */
    if((err = MP_InitSize(&q, a->used)) != true){
        return err;
    }

    q.used = a->used;
    q.sign = a->sign;
    w = 0;
    for(ix = a->used; ix --> 0;){
        MP_Digit t = 0;
        w = (w << (mp_word)MP_DIGIT_BIT) | (mp_word)a->digits[ix];
        if(w >= b){
            t = (MP_Digit)(w / b);
            w -= (mp_word)t * (mp_word)b;
        }
        q.digits[ix] = t;
    }

    if(d != NULL){
        *d = (MP_Digit)w;
    }

    if(c != NULL){
        MP_Clamp(&q);
        MP_Exch(&q, c);
    }
    MP_Clear(&q);

    return true;
}

bool MP_SubD(MP_Int *a, MP_Digit b, MP_Int *c){
    bool err;
    int oldused;

    /* fast path for a == c */
    if(a == c){
        if((c->sign == MP_NEG) && ((c->digits[0] + b) < MP_DIGIT_MAX)) {
            c->digits[0] += b;
            return true;
        }
        if((c->sign == MP_ZPOS) && (c->digits[0] > b)){
            c->digits[0] -= b;
            return true;
        }
    }

    /* grow c as required */
    if((err = MP_Grow(c, a->used + 1)) != true){
        return err;
    }

    /* if a is negative just do an unsigned
     * addition [with fudged signs]
     */
    if(a->sign == MP_NEG){
        MP_Int a_ = *a;
        a_.sign = MP_ZPOS;
        err     = MP_AddD(&a_, b, c);
        c->sign = MP_NEG;

        /* clamp */
        MP_Clamp(c);

        return err;
    }

    oldused = c->used;

    /* if a <= b simply fix the single digit */
    if(((a->used == 1) && (a->digits[0] <= b)) || MP_IsZero(a)){
        c->digits[0] = (a->used == 1) ? b - a->digits[0] : b;

        /* negative/1digit */
        c->sign = MP_NEG;
        c->used = 1;
    }else{
        int i;
        MP_Digit mu = b;

        /* positive/size */
        c->sign = MP_ZPOS;
        c->used = a->used;

        /* subtract digits, mu is carry */
        for(i = 0; i < a->used; i++){
            c->digits[i] = a->digits[i] - mu;
            mu = c->digits[i] >> (MP_SIZEOF_BITS(MP_Digit) - 1u);
            c->digits[i] &= MP_MASK;
        }
    }

    /* zero excess digits */
    MP_ZeroDigits(c->digits + c->used, oldused - c->used);

    MP_Clamp(c);
    return true;
}

bool MP_AddD(MP_Int *a, MP_Digit b, MP_Int *c){
    bool err;
    int oldused;

    /* fast path for a == c */
    if(a == c){
        if(!MP_IsNeg(c) && !MP_IsZero(c) && ((c->digits[0] + b) < MP_DIGIT_MAX)){
            c->digits[0] += b;
            return true;
        }
        if(MP_IsNeg(c) && (c->digits[0] > b)){
            c->digits[0] -= b;
            return true;
        }
    }

    /* grow c as required */
    if((err = MP_Grow(c, a->used + 1)) != true){
        return err;
    }

    /* if a is negative and |a| >= b, call c = |a| - b */
    if(MP_IsNeg(a) && ((a->used > 1) || (a->digits[0] >= b))){
        MP_Int a_ = *a;
        /* temporarily fix sign of a */
        a_.sign = MP_ZPOS;

        /* c = |a| - b */
        err = MP_SubD(&a_, b, c);

        /* fix sign  */
        c->sign = MP_NEG;

        /* clamp */
        MP_Clamp(c);

        return err;
    }

    /* old number of used digits in c */
    oldused = c->used;

    /* if a is positive */
    if(!MP_IsNeg(a)){
        /* add digits, mu is carry */
        int i;
        MP_Digit mu = b;
        for(i = 0; i < a->used; i++){
            c->digits[i] = a->digits[i] + mu;
            mu = c->digits[i] >> MP_DIGIT_BIT;
            c->digits[i] &= MP_MASK;
        }
        /* set final carry */
        c->digits[i] = mu;

        /* setup size */
        c->used = a->used + 1;
    }else{
        /* a was negative and |a| < b */
        c->used = 1;

        /* the result is a single digit */
        c->digits[0] = (a->used == 1) ? b - a->digits[0] : b;
    }

    /* sign always positive */
    c->sign = MP_ZPOS;

    /* now zero to oldused */
    MP_ZeroDigits(c->digits + c->used, oldused - c->used);
    MP_Clamp(c);

    return true;
}

bool MP_Add(MP_Int *mp0, MP_Int *mp1, MP_Int *c){
    MP_Int *a = mp0;
    MP_Int *b = mp1;
    if(a->sign == b->sign){
        c->sign = a->sign;
        return MP_Add_s(a, b, c);
    }

    if(MP_CmpMag(a, b) == MP_LT){
        a = mp1;
        b = mp0;
    }

    c->sign = a->sign;
    return MP_Sub_s(a, b, c);
}


bool MP_Sub(MP_Int *mp0, MP_Int *mp1, MP_Int *c){
    MP_Int *a = mp0;
    MP_Int *b = mp1;
    if(a->sign != b->sign){
        c->sign = a->sign;
        return MP_Add_s(a, b, c);
    }

    if(MP_CmpMag(a, b) == MP_LT){
        c->sign = (!MP_IsNeg(a) ? MP_NEG : MP_ZPOS);
        a = mp1;
        b = mp0;
    }else{
        c->sign = a->sign;
    }

    return MP_Sub_s(a, b, c);
}

/***************************************************************/
//                        String I/O                           //
/***************************************************************/
bool MP_ToRadix(MP_Int *a, std::string &str, int radix){
    MP_Int  t;
    MP_Digit d;
    bool err;
    int pushneg = 0;

    if((radix < 2) || (radix > 64)){
        return false;
    }

    /* quick out if its zero */
    if(MP_IsZero(a)){
        str = "0";
        return true;
    }

    if((err = MP_InitCopy(&t, a)) != true){
        return err;
    }

    /* if it is negative output a - */
    if(MP_IsNeg(&t)){
        /* we have to reverse our digits later... but not the - sign!! */
        pushneg = 1;
        t.sign = MP_ZPOS;
    }

    while(!MP_IsZero(&t)){
        if((err = MP_DivD(&t, (MP_Digit)radix, &t, &d)) != true){
            MP_Clear(&t);
            return err;
        }

        str += s_mp_radix_map[d];
    }

    /* reverse the digits of the string.  In this case _s points
     * to the first digit [excluding the sign] of the number
     */
    std::reverse(str.begin(), str.end());

    if(pushneg){
        str = std::string("-") + str;
    }

    MP_Clear(&t);
    return err;
}

bool MP_ReadRadix(MP_Int *a, const char *str, int radix){
    bool   err;
    int  sign = MP_ZPOS;

    /* make sure the radix is ok */
    if((radix < 2) || (radix > 64)){
        return false;
    }

    /* if the leading digit is a
     * minus set the sign to negative.
     */
    if(*str == '-'){
        ++str;
        sign = MP_NEG;
    }

    /* set the integer to the default of zero */
    MP_Zero(a);

    /* process each digit of the string */
    while(*str != '\0'){
        /* if the radix <= 36 the conversion is case insensitive
         * this allows numbers like 1AB and 1ab to represent the same  value
         * [e.g. in hex]
         */
        uint8_t y;
        char ch = (radix <= 36) ? (char)MP_TOUPPER((int)*str) : *str;
        unsigned pos = (unsigned)(ch - '+');
        if(MP_RADIX_MAP_REVERSE_SIZE <= pos){
            break;
        }
        y = s_mp_radix_map_reverse[pos];

        /* if the char was found in the map
         * and is less than the given radix add it
         * to the number, otherwise exit the loop.
         */
        if(y >= radix){
            break;
        }
        if((err = MP_MulD(a, (MP_Digit)radix, a)) != true){
            return err;
        }
        if((err = MP_AddD(a, y, a)) != true){
            return err;
        }
        ++str;
    }

    /* if an illegal character was found, fail. */
    if((*str != '\0') && (*str != '\r') && (*str != '\n')){
        return false;
    }

    /* set the sign only if a != 0 */
    if(!MP_IsZero(a)){
        a->sign = sign;
    }
    return true;
}
