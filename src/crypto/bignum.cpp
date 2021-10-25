#include <bignum.h>
#include <types.h>
#include <cstring>
#include <limits.h>
#include <types.h>

#define ciL    (sizeof(uintt))         /* chars in limb  */
#define biL    (ciL << 3)               /* bits  in limb  */
#define biH    (ciL << 2)               /* half limb size */

#define MPI_SIZE_T_MAX  ( (size_t) -1 ) /* SIZE_T_MAX is not standard */

#define BIGNUM_MPI_MAX_SIZE 1024
#define BIGNUM_MPI_WINDOW_SIZE 6
#define BIGNUM_MPI_MAX_BITS ( 8 * BIGNUM_MPI_MAX_SIZE )

#define BIGNUM_MPI_MAX_LIMBS 10000
#define BIGNUM_ERR_MPI_ALLOC_FAILED -1
#define BIGNUM_ERR_MPI_INVALID_CHARACTER -1
#define BIGNUM_ERR_MPI_BAD_INPUT_DATA -1
#define BIGNUM_ERR_MPI_BUFFER_TOO_SMALL -1
#define BIGNUM_ERR_ERROR_CORRUPTION_DETECTED -1
#define BIGNUM_ERR_MPI_NEGATIVE_VALUE -1
#define BIGNUM_ERR_MPI_NOT_ACCEPTABLE -1
#define BIGNUM_ERR_MPI_DIVISION_BY_ZERO -1

#define MPI_VALIDATE_RET(...)
#define BIGNUM_INTERNAL_VALIDATE_RET(...)
#define MPI_VALIDATE(...)

#define BIGNUM_MPI_CHK(f)       \
    do                           \
    {                            \
        if( ( ret = (f) ) != 0 ) \
            goto cleanup;        \
    } while( 0 )

/*
 * Convert between bits/chars and number of limbs
 * Divide first in order to avoid potential overflows
 */
#define BITS_TO_LIMBS(i)  ( (i) / biL + ( (i) % biL != 0 ) )
#define CHARS_TO_LIMBS(i) ( (i) / ciL + ( (i) % ciL != 0 ) )

#define MULADDC_INIT                    \
{                                       \
    udbl r;                           \
    uintt r0, r1;

#define MULADDC_CORE                    \
    r   = *(s++) * (udbl) b;          \
    r0  = (uintt) r;                   \
    r1  = (uintt)( r >> biL );         \
    r0 += c;  r1 += (r0 <  c);          \
    r0 += *d; r1 += (r0 < *d);          \
    c = r1; *(d++) = r0;

#define MULADDC_STOP                    \
}

typedef enum {
    BIGNUM_MPI_GEN_PRIME_FLAG_DH =      0x0001, /**< (X-1)/2 is prime too */
    BIGNUM_MPI_GEN_PRIME_FLAG_LOW_ERR = 0x0002, /**< lower error rate from 2<sup>-80</sup> to 2<sup>-128</sup> */
} mbedtls_mpi_gen_prime_flag_t;

int Bignum_mul_int( Bignum *X, const Bignum *A, uintt b );
int Bignum_add_int( Bignum *X, const Bignum *A, intt b );
int Bignum_mod_int( uintt *r, const Bignum *A, intt b );
int Bignum_div_int( Bignum *Q, Bignum *R, const Bignum *A, intt b );
int Bignum_cmp_int( const Bignum *X, intt z );

/* Implementation that should never be optimized out by the compiler */
static void Bignum_zeroize(uintt *v, size_t n){
    memset(v, 0, ciL * n );
}

/*
 * Initialize one MPI
 */
void Bignum_init( Bignum *X )
{
    X->s = 1;
    X->n = 0;
    X->p = NULL;
}

/*
 * Unallocate one MPI
 */
void Bignum_free( Bignum *X )
{
    if( X == NULL )
        return;

    if( X->p != NULL )
    {
        Bignum_zeroize( X->p, X->n );
        AllocatorFree( X->p );
    }

    X->s = 1;
    X->n = 0;
    X->p = NULL;
}

/*
 * Enlarge to the specified number of limbs
 */
int Bignum_grow( Bignum *X, size_t nblimbs )
{
    uintt *p;
    MPI_VALIDATE_RET( X != NULL );

    if( nblimbs > BIGNUM_MPI_MAX_LIMBS )
        return( BIGNUM_ERR_MPI_ALLOC_FAILED );

    if( X->n < nblimbs )
    {
        if( ( p = (uintt*)AllocatorCalloc( nblimbs, ciL ) ) == NULL )
            return( BIGNUM_ERR_MPI_ALLOC_FAILED );

        if( X->p != NULL )
        {
            memcpy( p, X->p, X->n * ciL );
            Bignum_zeroize( X->p, X->n );
            AllocatorFree( X->p );
        }

        X->n = nblimbs;
        X->p = p;
    }

    return( 0 );
}

/*
 * Resize down as much as possible,
 * while keeping at least the specified number of limbs
 */
int Bignum_shrink( Bignum *X, size_t nblimbs )
{
    uintt *p;
    size_t i;
    MPI_VALIDATE_RET( X != NULL );

    if( nblimbs > BIGNUM_MPI_MAX_LIMBS )
        return( BIGNUM_ERR_MPI_ALLOC_FAILED );

    /* Actually resize up if there are currently fewer than nblimbs limbs. */
    if( X->n <= nblimbs )
        return( Bignum_grow( X, nblimbs ) );
    /* After this point, then X->n > nblimbs and in particular X->n > 0. */

    for( i = X->n - 1; i > 0; i-- )
        if( X->p[i] != 0 )
            break;
    i++;

    if( i < nblimbs )
        i = nblimbs;

    if( ( p = (uintt*)AllocatorCalloc( i, ciL ) ) == NULL )
        return( BIGNUM_ERR_MPI_ALLOC_FAILED );

    if( X->p != NULL )
    {
        memcpy( p, X->p, i * ciL );
        Bignum_zeroize( X->p, X->n );
        AllocatorFree( X->p );
    }

    X->n = i;
    X->p = p;

    return( 0 );
}

/* Resize X to have exactly n limbs and set it to 0. */
static int Bignum_resize_clear( Bignum *X, size_t limbs )
{
    if( limbs == 0 )
    {
        Bignum_free( X );
        return( 0 );
    }
    else if( X->n == limbs )
    {
        memset( X->p, 0, limbs * ciL );
        X->s = 1;
        return( 0 );
    }
    else
    {
        Bignum_free( X );
        return( Bignum_grow( X, limbs ) );
    }
}

/*
 * Copy the contents of Y into X.
 *
 * This function is not constant-time. Leading zeros in Y may be removed.
 *
 * Ensure that X does not shrink. This is not guaranteed by the public API,
 * but some code in the bignum module relies on this property, for example
 * in Bignum_exp_mod().
 */
int Bignum_copy( Bignum *X, const Bignum *Y )
{
    int ret = 0;
    size_t i;
    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( Y != NULL );

    if( X == Y )
        return( 0 );

    if( Y->n == 0 )
    {
        if( X->n != 0 )
        {
            X->s = 1;
            memset( X->p, 0, X->n * ciL );
        }
        return( 0 );
    }

    for( i = Y->n - 1; i > 0; i-- )
        if( Y->p[i] != 0 )
            break;
    i++;

    X->s = Y->s;

    if( X->n < i )
    {
        BIGNUM_MPI_CHK( Bignum_grow( X, i ) );
    }
    else
    {
        memset( X->p + i, 0, ( X->n - i ) * ciL );
    }

    memcpy( X->p, Y->p, i * ciL );

cleanup:

    return( ret );
}

/*
 * Swap the contents of X and Y
 */
void Bignum_swap( Bignum *X, Bignum *Y )
{
    Bignum T;
    MPI_VALIDATE( X != NULL );
    MPI_VALIDATE( Y != NULL );

    memcpy( &T,  X, sizeof( Bignum ) );
    memcpy(  X,  Y, sizeof( Bignum ) );
    memcpy(  Y, &T, sizeof( Bignum ) );
}

/**
 * Select between two sign values in constant-time.
 *
 * This is functionally equivalent to second ? a : b but uses only bit
 * operations in order to avoid branches.
 *
 * \param[in] a         The first sign; must be either +1 or -1.
 * \param[in] b         The second sign; must be either +1 or -1.
 * \param[in] second    Must be either 1 (return b) or 0 (return a).
 *
 * \return The selected sign value.
 */
static int mpi_safe_cond_select_sign( int a, int b, unsigned char second )
{
    /* In order to avoid questions about what we can reasonnably assume about
     * the representations of signed integers, move everything to unsigned
     * by taking advantage of the fact that a and b are either +1 or -1. */
    unsigned ua = a + 1;
    unsigned ub = b + 1;

    /* second was 0 or 1, mask is 0 or 2 as are ua and ub */
    const unsigned mask = second << 1;

    /* select ua or ub */
    unsigned ur = ( ua & ~mask ) | ( ub & mask );

    /* ur is now 0 or 2, convert back to -1 or +1 */
    return( (int) ur - 1 );
}

/*
 * Conditionally assign dest = src, without leaking information
 * about whether the assignment was made or not.
 * dest and src must be arrays of limbs of size n.
 * assign must be 0 or 1.
 */
static void mpi_safe_cond_assign( size_t n,
                                  uintt *dest,
                                  const uintt *src,
                                  unsigned char assign )
{
    size_t i;

    /* MSVC has a warning about unary minus on unsigned integer types,
     * but this is well-defined and precisely what we want to do here. */
#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4146 )
#endif

    /* all-bits 1 if assign is 1, all-bits 0 if assign is 0 */
    const uintt mask = -assign;

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

    for( i = 0; i < n; i++ )
        dest[i] = ( src[i] & mask ) | ( dest[i] & ~mask );
}

/*
 * Conditionally assign X = Y, without leaking information
 * about whether the assignment was made or not.
 * (Leaking information about the respective sizes of X and Y is ok however.)
 */
int Bignum_safe_cond_assign( Bignum *X, const Bignum *Y, unsigned char assign )
{
    int ret = 0;
    size_t i;
    uintt limb_mask;
    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( Y != NULL );

    /* MSVC has a warning about unary minus on unsigned integer types,
     * but this is well-defined and precisely what we want to do here. */
#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4146 )
#endif

    /* make sure assign is 0 or 1 in a time-constant manner */
    assign = (assign | (unsigned char)-assign) >> (sizeof( assign ) * 8 - 1);
    /* all-bits 1 if assign is 1, all-bits 0 if assign is 0 */
    limb_mask = -assign;

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

    BIGNUM_MPI_CHK( Bignum_grow( X, Y->n ) );

    X->s = mpi_safe_cond_select_sign( X->s, Y->s, assign );

    mpi_safe_cond_assign( Y->n, X->p, Y->p, assign );

    for( i = Y->n; i < X->n; i++ )
        X->p[i] &= ~limb_mask;

cleanup:
    return( ret );
}

/*
 * Conditionally swap X and Y, without leaking information
 * about whether the swap was made or not.
 * Here it is not ok to simply swap the pointers, which whould lead to
 * different memory access patterns when X and Y are used afterwards.
 */
int Bignum_safe_cond_swap( Bignum *X, Bignum *Y, unsigned char swap )
{
    int ret = 0, s;
    size_t i;
    uintt limb_mask;
    uintt tmp;
    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( Y != NULL );

    if( X == Y )
        return( 0 );

    /* MSVC has a warning about unary minus on unsigned integer types,
     * but this is well-defined and precisely what we want to do here. */
#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4146 )
#endif

    /* make sure swap is 0 or 1 in a time-constant manner */
    swap = (swap | (unsigned char)-swap) >> (sizeof( swap ) * 8 - 1);
    /* all-bits 1 if swap is 1, all-bits 0 if swap is 0 */
    limb_mask = -swap;

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

    BIGNUM_MPI_CHK( Bignum_grow( X, Y->n ) );
    BIGNUM_MPI_CHK( Bignum_grow( Y, X->n ) );

    s = X->s;
    X->s = mpi_safe_cond_select_sign( X->s, Y->s, swap );
    Y->s = mpi_safe_cond_select_sign( Y->s, s, swap );


    for( i = 0; i < X->n; i++ )
    {
        tmp = X->p[i];
        X->p[i] = ( X->p[i] & ~limb_mask ) | ( Y->p[i] & limb_mask );
        Y->p[i] = ( Y->p[i] & ~limb_mask ) | (     tmp & limb_mask );
    }

cleanup:
    return( ret );
}

/*
 * Set value from integer
 */
int Bignum_lset( Bignum *X, intt z )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    MPI_VALIDATE_RET( X != NULL );

    BIGNUM_MPI_CHK( Bignum_grow( X, 1 ) );
    memset( X->p, 0, X->n * ciL );

    X->p[0] = ( z < 0 ) ? -z : z;
    X->s    = ( z < 0 ) ? -1 : 1;

cleanup:

    return( ret );
}

/*
 * Get a specific bit
 */
int Bignum_get_bit( const Bignum *X, size_t pos )
{
    MPI_VALIDATE_RET( X != NULL );

    if( X->n * biL <= pos )
        return( 0 );

    return( ( X->p[pos / biL] >> ( pos % biL ) ) & 0x01 );
}

/* Get a specific byte, without range checks. */
#define GET_BYTE( X, i )                                \
    ( ( ( X )->p[( i ) / ciL] >> ( ( ( i ) % ciL ) * 8 ) ) & 0xff )

/*
 * Set a bit to a specific value of 0 or 1
 */
int Bignum_set_bit( Bignum *X, size_t pos, unsigned char val )
{
    int ret = 0;
    size_t off = pos / biL;
    size_t idx = pos % biL;
    MPI_VALIDATE_RET( X != NULL );

    if( val != 0 && val != 1 )
        return( BIGNUM_ERR_MPI_BAD_INPUT_DATA );

    if( X->n * biL <= pos )
    {
        if( val == 0 )
            return( 0 );

        BIGNUM_MPI_CHK( Bignum_grow( X, off + 1 ) );
    }

    X->p[off] &= ~( (uintt) 0x01 << idx );
    X->p[off] |= (uintt) val << idx;

cleanup:

    return( ret );
}

/*
 * Return the number of less significant zero-bits
 */
size_t Bignum_lsb( const Bignum *X )
{
    size_t i, j, count = 0;
    BIGNUM_INTERNAL_VALIDATE_RET( X != NULL, 0 );

    for( i = 0; i < X->n; i++ )
        for( j = 0; j < biL; j++, count++ )
            if( ( ( X->p[i] >> j ) & 1 ) != 0 )
                return( count );

    return( 0 );
}

/*
 * Count leading zero bits in a given integer
 */
static size_t mbedtls_clz( const uintt x )
{
    size_t j;
    uintt mask = (uintt) 1 << (biL - 1);

    for( j = 0; j < biL; j++ )
    {
        if( x & mask ) break;

        mask >>= 1;
    }

    return j;
}

/*
 * Return the number of bits
 */
size_t Bignum_bitlen( const Bignum *X )
{
    size_t i, j;

    if( X->n == 0 )
        return( 0 );

    for( i = X->n - 1; i > 0; i-- )
        if( X->p[i] != 0 )
            break;

    j = biL - mbedtls_clz( X->p[i] );

    return( ( i * biL ) + j );
}

/*
 * Return the total size in bytes
 */
size_t Bignum_size( const Bignum *X )
{
    return( ( Bignum_bitlen( X ) + 7 ) >> 3 );
}

/*
 * Convert an ASCII character to digit value
 */
static int mpi_get_digit( uintt *d, int radix, char c )
{
    *d = 255;

    if( c >= 0x30 && c <= 0x39 ) *d = c - 0x30;
    if( c >= 0x41 && c <= 0x46 ) *d = c - 0x37;
    if( c >= 0x61 && c <= 0x66 ) *d = c - 0x57;

    if( *d >= (uintt) radix )
        return( BIGNUM_ERR_MPI_INVALID_CHARACTER );

    return( 0 );
}

/*
 * Import from an ASCII string
 */
int Bignum_read_string( Bignum *X, int radix, const char *s )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    size_t i, j, slen, n;
    int sign = 1;
    uintt d;
    Bignum T;
    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( s != NULL );

    if( radix < 2 || radix > 16 )
        return( BIGNUM_ERR_MPI_BAD_INPUT_DATA );

    Bignum_init( &T );

    if( s[0] == 0 )
    {
        Bignum_free( X );
        return( 0 );
    }

    if( s[0] == '-' )
    {
        ++s;
        sign = -1;
    }

    slen = strlen( s );

    if( radix == 16 )
    {
        if( slen > MPI_SIZE_T_MAX >> 2 )
            return( BIGNUM_ERR_MPI_BAD_INPUT_DATA );

        n = BITS_TO_LIMBS( slen << 2 );

        BIGNUM_MPI_CHK( Bignum_grow( X, n ) );
        BIGNUM_MPI_CHK( Bignum_lset( X, 0 ) );

        for( i = slen, j = 0; i > 0; i--, j++ )
        {
            BIGNUM_MPI_CHK( mpi_get_digit( &d, radix, s[i - 1] ) );
            X->p[j / ( 2 * ciL )] |= d << ( ( j % ( 2 * ciL ) ) << 2 );
        }
    }
    else
    {
        BIGNUM_MPI_CHK( Bignum_lset( X, 0 ) );

        for( i = 0; i < slen; i++ )
        {
            BIGNUM_MPI_CHK( mpi_get_digit( &d, radix, s[i] ) );
            BIGNUM_MPI_CHK( Bignum_mul_int( &T, X, radix ) );
            BIGNUM_MPI_CHK( Bignum_add_int( X, &T, d ) );
        }
    }

    if( sign < 0 && Bignum_bitlen( X ) != 0 )
        X->s = -1;

cleanup:

    Bignum_free( &T );

    return( ret );
}

/*
 * Helper to write the digits high-order first.
 */
static int mpi_write_hlp( Bignum *X, int radix,
                          char **p, const size_t buflen )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    uintt r;
    size_t length = 0;
    char *p_end = *p + buflen;

    do
    {
        if( length >= buflen )
        {
            return( BIGNUM_ERR_MPI_BUFFER_TOO_SMALL );
        }

        BIGNUM_MPI_CHK( Bignum_mod_int( &r, X, radix ) );
        BIGNUM_MPI_CHK( Bignum_div_int( X, NULL, X, radix ) );
        /*
         * Write the residue in the current position, as an ASCII character.
         */
        if( r < 0xA )
            *(--p_end) = (char)( '0' + r );
        else
            *(--p_end) = (char)( 'A' + ( r - 0xA ) );

        length++;
    } while( Bignum_cmp_int( X, 0 ) != 0 );

    memmove( *p, p_end, length );
    *p += length;

cleanup:

    return( ret );
}

/*
 * Export into an ASCII string
 */
int Bignum_write_string( const Bignum *X, int radix, char *buf,
                         size_t buflen, size_t *olen )
{
    int ret = 0;
    size_t n;
    char *p;
    Bignum T;
    MPI_VALIDATE_RET( X    != NULL );
    MPI_VALIDATE_RET( olen != NULL );
    MPI_VALIDATE_RET( buflen == 0 || buf != NULL );

    if( radix < 2 || radix > 16 )
        return( BIGNUM_ERR_MPI_BAD_INPUT_DATA );

    n = Bignum_bitlen( X ); /* Number of bits necessary to present `n`. */
    if( radix >=  4 ) n >>= 1;   /* Number of 4-adic digits necessary to present
                                  * `n`. If radix > 4, this might be a strict
                                  * overapproximation of the number of
                                  * radix-adic digits needed to present `n`. */
    if( radix >= 16 ) n >>= 1;   /* Number of hexadecimal digits necessary to
                                  * present `n`. */

    n += 1; /* Terminating null byte */
    n += 1; /* Compensate for the divisions above, which round down `n`
             * in case it's not even. */
    n += 1; /* Potential '-'-sign. */
    n += ( n & 1 ); /* Make n even to have enough space for hexadecimal writing,
                     * which always uses an even number of hex-digits. */

    if( buflen < n )
    {
        *olen = n;
        return( BIGNUM_ERR_MPI_BUFFER_TOO_SMALL );
    }

    p = buf;
    Bignum_init( &T );

    if( X->s == -1 )
    {
        *p++ = '-';
        buflen--;
    }

    if( radix == 16 )
    {
        int c;
        size_t i, j, k;

        for( i = X->n, k = 0; i > 0; i-- )
        {
            for( j = ciL; j > 0; j-- )
            {
                c = ( X->p[i - 1] >> ( ( j - 1 ) << 3) ) & 0xFF;

                if( c == 0 && k == 0 && ( i + j ) != 2 )
                    continue;

                *(p++) = "0123456789ABCDEF" [c / 16];
                *(p++) = "0123456789ABCDEF" [c % 16];
                k = 1;
            }
        }
    }
    else
    {
        BIGNUM_MPI_CHK( Bignum_copy( &T, X ) );

        if( T.s == -1 )
            T.s = 1;

        BIGNUM_MPI_CHK( mpi_write_hlp( &T, radix, &p, buflen ) );
    }

    *p++ = '\0';
    *olen = p - buf;

cleanup:

    Bignum_free( &T );

    return( ret );
}

/* Convert a big-endian byte array aligned to the size of uintt
 * into the storage form used by Bignum. */

static uintt mpi_uint_bigendian_to_host_c( uintt x )
{
    uint8_t i;
    unsigned char *x_ptr;
    uintt tmp = 0;

    for( i = 0, x_ptr = (unsigned char*) &x; i < ciL; i++, x_ptr++ )
    {
        tmp <<= CHAR_BIT;
        tmp |= (uintt) *x_ptr;
    }

    return( tmp );
}

static uintt mpi_uint_bigendian_to_host( uintt x )
{
#if defined(__BYTE_ORDER__)

/* Nothing to do on bigendian systems. */
#if ( __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ )
    return( x );
#endif /* __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ */

#if ( __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ )

/* For GCC and Clang, have builtins for byte swapping. */
#if defined(__GNUC__) && defined(__GNUC_PREREQ)
#if __GNUC_PREREQ(4,3)
#define have_bswap
#endif
#endif

#if defined(__clang__) && defined(__has_builtin)
#if __has_builtin(__builtin_bswap32)  &&                 \
    __has_builtin(__builtin_bswap64)
#define have_bswap
#endif
#endif

#if defined(have_bswap)
    /* The compiler is hopefully able to statically evaluate this! */
    switch( sizeof(uintt) )
    {
        case 4:
            return( __builtin_bswap32(x) );
        case 8:
            return( __builtin_bswap64(x) );
    }
#endif
#endif /* __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ */
#endif /* __BYTE_ORDER__ */

    /* Fall back to C-based reordering if we don't know the byte order
     * or we couldn't use a compiler-specific builtin. */
    return( mpi_uint_bigendian_to_host_c( x ) );
}

static void mpi_bigendian_to_host( uintt * const p, size_t limbs )
{
    uintt *cur_limb_left;
    uintt *cur_limb_right;
    if( limbs == 0 )
        return;

    /*
     * Traverse limbs and
     * - adapt byte-order in each limb
     * - swap the limbs themselves.
     * For that, simultaneously traverse the limbs from left to right
     * and from right to left, as long as the left index is not bigger
     * than the right index (it's not a problem if limbs is odd and the
     * indices coincide in the last iteration).
     */
    for( cur_limb_left = p, cur_limb_right = p + ( limbs - 1 );
         cur_limb_left <= cur_limb_right;
         cur_limb_left++, cur_limb_right-- )
    {
        uintt tmp;
        /* Note that if cur_limb_left == cur_limb_right,
         * this code effectively swaps the bytes only once. */
        tmp             = mpi_uint_bigendian_to_host( *cur_limb_left  );
        *cur_limb_left  = mpi_uint_bigendian_to_host( *cur_limb_right );
        *cur_limb_right = tmp;
    }
}

/*
 * Import X from unsigned binary data, little endian
 */
int Bignum_read_binary_le( Bignum *X,
                                const unsigned char *buf, size_t buflen )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    size_t i;
    size_t const limbs = CHARS_TO_LIMBS( buflen );

    /* Ensure that target MPI has exactly the necessary number of limbs */
    BIGNUM_MPI_CHK( Bignum_resize_clear( X, limbs ) );

    for( i = 0; i < buflen; i++ )
        X->p[i / ciL] |= ((uintt) buf[i]) << ((i % ciL) << 3);

cleanup:

    /*
     * This function is also used to import keys. However, wiping the buffers
     * upon failure is not necessary because failure only can happen before any
     * input is copied.
     */
    return( ret );
}

/*
 * Import X from unsigned binary data, big endian
 */
int Bignum_read_binary( Bignum *X, const unsigned char *buf, size_t buflen )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    size_t const limbs    = CHARS_TO_LIMBS( buflen );
    size_t const overhead = ( limbs * ciL ) - buflen;
    unsigned char *Xp;

    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( buflen == 0 || buf != NULL );

    /* Ensure that target MPI has exactly the necessary number of limbs */
    BIGNUM_MPI_CHK( Bignum_resize_clear( X, limbs ) );

    /* Avoid calling `memcpy` with NULL source or destination argument,
     * even if buflen is 0. */
    if( buflen != 0 )
    {
        Xp = (unsigned char*) X->p;
        memcpy( Xp + overhead, buf, buflen );

        mpi_bigendian_to_host( X->p, limbs );
    }

cleanup:

    /*
     * This function is also used to import keys. However, wiping the buffers
     * upon failure is not necessary because failure only can happen before any
     * input is copied.
     */
    return( ret );
}

/*
 * Export X into unsigned binary data, little endian
 */
int Bignum_write_binary_le( const Bignum *X,
                                 unsigned char *buf, size_t buflen )
{
    size_t stored_bytes = X->n * ciL;
    size_t bytes_to_copy;
    size_t i;

    if( stored_bytes < buflen )
    {
        bytes_to_copy = stored_bytes;
    }
    else
    {
        bytes_to_copy = buflen;

        /* The output buffer is smaller than the allocated size of X.
         * However X may fit if its leading bytes are zero. */
        for( i = bytes_to_copy; i < stored_bytes; i++ )
        {
            if( GET_BYTE( X, i ) != 0 )
                return( BIGNUM_ERR_MPI_BUFFER_TOO_SMALL );
        }
    }

    for( i = 0; i < bytes_to_copy; i++ )
        buf[i] = GET_BYTE( X, i );

    if( stored_bytes < buflen )
    {
        /* Write trailing 0 bytes */
        memset( buf + stored_bytes, 0, buflen - stored_bytes );
    }

    return( 0 );
}

/*
 * Export X into unsigned binary data, big endian
 */
int Bignum_write_binary( const Bignum *X,
                              unsigned char *buf, size_t buflen )
{
    size_t stored_bytes;
    size_t bytes_to_copy;
    unsigned char *p;
    size_t i;

    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( buflen == 0 || buf != NULL );

    stored_bytes = X->n * ciL;

    if( stored_bytes < buflen )
    {
        /* There is enough space in the output buffer. Write initial
         * null bytes and record the position at which to start
         * writing the significant bytes. In this case, the execution
         * trace of this function does not depend on the value of the
         * number. */
        bytes_to_copy = stored_bytes;
        p = buf + buflen - stored_bytes;
        memset( buf, 0, buflen - stored_bytes );
    }
    else
    {
        /* The output buffer is smaller than the allocated size of X.
         * However X may fit if its leading bytes are zero. */
        bytes_to_copy = buflen;
        p = buf;
        for( i = bytes_to_copy; i < stored_bytes; i++ )
        {
            if( GET_BYTE( X, i ) != 0 )
                return( BIGNUM_ERR_MPI_BUFFER_TOO_SMALL );
        }
    }

    for( i = 0; i < bytes_to_copy; i++ )
        p[bytes_to_copy - i - 1] = GET_BYTE( X, i );

    return( 0 );
}

/*
 * Left-shift: X <<= count
 */
int Bignum_shift_l( Bignum *X, size_t count )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    size_t i, v0, t1;
    uintt r0 = 0, r1;
    MPI_VALIDATE_RET( X != NULL );

    v0 = count / (biL    );
    t1 = count & (biL - 1);

    i = Bignum_bitlen( X ) + count;

    if( X->n * biL < i )
        BIGNUM_MPI_CHK( Bignum_grow( X, BITS_TO_LIMBS( i ) ) );

    ret = 0;

    /*
     * shift by count / limb_size
     */
    if( v0 > 0 )
    {
        for( i = X->n; i > v0; i-- )
            X->p[i - 1] = X->p[i - v0 - 1];

        for( ; i > 0; i-- )
            X->p[i - 1] = 0;
    }

    /*
     * shift by count % limb_size
     */
    if( t1 > 0 )
    {
        for( i = v0; i < X->n; i++ )
        {
            r1 = X->p[i] >> (biL - t1);
            X->p[i] <<= t1;
            X->p[i] |= r0;
            r0 = r1;
        }
    }

cleanup:

    return( ret );
}

/*
 * Right-shift: X >>= count
 */
int Bignum_shift_r( Bignum *X, size_t count )
{
    size_t i, v0, v1;
    uintt r0 = 0, r1;
    MPI_VALIDATE_RET( X != NULL );

    v0 = count /  biL;
    v1 = count & (biL - 1);

    if( v0 > X->n || ( v0 == X->n && v1 > 0 ) )
        return Bignum_lset( X, 0 );

    /*
     * shift by count / limb_size
     */
    if( v0 > 0 )
    {
        for( i = 0; i < X->n - v0; i++ )
            X->p[i] = X->p[i + v0];

        for( ; i < X->n; i++ )
            X->p[i] = 0;
    }

    /*
     * shift by count % limb_size
     */
    if( v1 > 0 )
    {
        for( i = X->n; i > 0; i-- )
        {
            r1 = X->p[i - 1] << (biL - v1);
            X->p[i - 1] >>= v1;
            X->p[i - 1] |= r0;
            r0 = r1;
        }
    }

    return( 0 );
}

/*
 * Compare unsigned values
 */
int Bignum_cmp_abs( const Bignum *X, const Bignum *Y )
{
    size_t i, j;
    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( Y != NULL );

    for( i = X->n; i > 0; i-- )
        if( X->p[i - 1] != 0 )
            break;

    for( j = Y->n; j > 0; j-- )
        if( Y->p[j - 1] != 0 )
            break;

    if( i == 0 && j == 0 )
        return( 0 );

    if( i > j ) return(  1 );
    if( j > i ) return( -1 );

    for( ; i > 0; i-- )
    {
        if( X->p[i - 1] > Y->p[i - 1] ) return(  1 );
        if( X->p[i - 1] < Y->p[i - 1] ) return( -1 );
    }

    return( 0 );
}

/*
 * Compare signed values
 */
int Bignum_cmp_mpi( const Bignum *X, const Bignum *Y )
{
    size_t i, j;
    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( Y != NULL );

    for( i = X->n; i > 0; i-- )
        if( X->p[i - 1] != 0 )
            break;

    for( j = Y->n; j > 0; j-- )
        if( Y->p[j - 1] != 0 )
            break;

    if( i == 0 && j == 0 )
        return( 0 );

    if( i > j ) return(  X->s );
    if( j > i ) return( -Y->s );

    if( X->s > 0 && Y->s < 0 ) return(  1 );
    if( Y->s > 0 && X->s < 0 ) return( -1 );

    for( ; i > 0; i-- )
    {
        if( X->p[i - 1] > Y->p[i - 1] ) return(  X->s );
        if( X->p[i - 1] < Y->p[i - 1] ) return( -X->s );
    }

    return( 0 );
}

/** Decide if an integer is less than the other, without branches.
 *
 * \param x         First integer.
 * \param y         Second integer.
 *
 * \return          1 if \p x is less than \p y, 0 otherwise
 */
static unsigned ct_lt_mpi_uint( const uintt x,
        const uintt y )
{
    uintt ret;
    uintt cond;

    /*
     * Check if the most significant bits (MSB) of the operands are different.
     */
    cond = ( x ^ y );
    /*
     * If the MSB are the same then the difference x-y will be negative (and
     * have its MSB set to 1 during conversion to unsigned) if and only if x<y.
     */
    ret = ( x - y ) & ~cond;
    /*
     * If the MSB are different, then the operand with the MSB of 1 is the
     * bigger. (That is if y has MSB of 1, then x<y is true and it is false if
     * the MSB of y is 0.)
     */
    ret |= y & cond;


    ret = ret >> ( biL - 1 );

    return (unsigned) ret;
}

/*
 * Compare signed values in constant time
 */
int Bignum_lt_mpi_ct( const Bignum *X, const Bignum *Y, unsigned *ret ){
    size_t i;
    /* The value of any of these variables is either 0 or 1 at all times. */
    unsigned cond, done, X_is_negative, Y_is_negative;

    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( Y != NULL );
    MPI_VALIDATE_RET( ret != NULL );

    if( X->n != Y->n )
        return BIGNUM_ERR_MPI_BAD_INPUT_DATA;

    /*
     * Set sign_N to 1 if N >= 0, 0 if N < 0.
     * We know that N->s == 1 if N >= 0 and N->s == -1 if N < 0.
     */
    X_is_negative = ( X->s & 2 ) >> 1;
    Y_is_negative = ( Y->s & 2 ) >> 1;

    /*
     * If the signs are different, then the positive operand is the bigger.
     * That is if X is negative (X_is_negative == 1), then X < Y is true and it
     * is false if X is positive (X_is_negative == 0).
     */
    cond = ( X_is_negative ^ Y_is_negative );
    *ret = cond & X_is_negative;

    /*
     * This is a constant-time function. We might have the result, but we still
     * need to go through the loop. Record if we have the result already.
     */
    done = cond;

    for( i = X->n; i > 0; i-- )
    {
        /*
         * If Y->p[i - 1] < X->p[i - 1] then X < Y is true if and only if both
         * X and Y are negative.
         *
         * Again even if we can make a decision, we just mark the result and
         * the fact that we are done and continue looping.
         */
        cond = ct_lt_mpi_uint( Y->p[i - 1], X->p[i - 1] );
        *ret |= cond & ( 1 - done ) & X_is_negative;
        done |= cond;

        /*
         * If X->p[i - 1] < Y->p[i - 1] then X < Y is true if and only if both
         * X and Y are positive.
         *
         * Again even if we can make a decision, we just mark the result and
         * the fact that we are done and continue looping.
         */
        cond = ct_lt_mpi_uint( X->p[i - 1], Y->p[i - 1] );
        *ret |= cond & ( 1 - done ) & ( 1 - X_is_negative );
        done |= cond;
    }

    return( 0 );
}

/*
 * Compare signed values
 */
int Bignum_cmp_int( const Bignum *X, intt z )
{
    Bignum Y;
    uintt p[1];
    MPI_VALIDATE_RET( X != NULL );

    *p  = ( z < 0 ) ? -z : z;
    Y.s = ( z < 0 ) ? -1 : 1;
    Y.n = 1;
    Y.p = p;

    return( Bignum_cmp_mpi( X, &Y ) );
}

/*
 * Unsigned addition: X = |A| + |B|  (HAC 14.7)
 */
int Bignum_add_abs( Bignum *X, const Bignum *A, const Bignum *B )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    size_t i, j;
    uintt *o, *p, c, tmp;
    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( A != NULL );
    MPI_VALIDATE_RET( B != NULL );

    if( X == B )
    {
        const Bignum *T = A; A = X; B = T;
    }

    if( X != A )
        BIGNUM_MPI_CHK( Bignum_copy( X, A ) );

    /*
     * X should always be positive as a result of unsigned additions.
     */
    X->s = 1;

    for( j = B->n; j > 0; j-- )
        if( B->p[j - 1] != 0 )
            break;

    BIGNUM_MPI_CHK( Bignum_grow( X, j ) );

    o = B->p; p = X->p; c = 0;

    /*
     * tmp is used because it might happen that p == o
     */
    for( i = 0; i < j; i++, o++, p++ )
    {
        tmp= *o;
        *p +=  c; c  = ( *p <  c );
        *p += tmp; c += ( *p < tmp );
    }

    while( c != 0 )
    {
        if( i >= X->n )
        {
            BIGNUM_MPI_CHK( Bignum_grow( X, i + 1 ) );
            p = X->p + i;
        }

        *p += c; c = ( *p < c ); i++; p++;
    }

cleanup:

    return( ret );
}

/**
 * Helper for Bignum subtraction.
 *
 * Calculate l - r where l and r have the same size.
 * This function operates modulo (2^ciL)^n and returns the carry
 * (1 if there was a wraparound, i.e. if `l < r`, and 0 otherwise).
 *
 * d may be aliased to l or r.
 *
 * \param n             Number of limbs of \p d, \p l and \p r.
 * \param[out] d        The result of the subtraction.
 * \param[in] l         The left operand.
 * \param[in] r         The right operand.
 *
 * \return              1 if `l < r`.
 *                      0 if `l >= r`.
 */
static uintt mpi_sub_hlp( size_t n,
                                     uintt *d,
                                     const uintt *l,
                                     const uintt *r )
{
    size_t i;
    uintt c = 0, t, z;

    for( i = 0; i < n; i++ )
    {
        z = ( l[i] <  c );    t = l[i] - c;
        c = ( t < r[i] ) + z; d[i] = t - r[i];
    }

    return( c );
}

/*
 * Unsigned subtraction: X = |A| - |B|  (HAC 14.9, 14.10)
 */
int Bignum_sub_abs( Bignum *X, const Bignum *A, const Bignum *B )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    size_t n;
    uintt carry;
    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( A != NULL );
    MPI_VALIDATE_RET( B != NULL );

    for( n = B->n; n > 0; n-- )
        if( B->p[n - 1] != 0 )
            break;
    if( n > A->n )
    {
        /* B >= (2^ciL)^n > A */
        ret = BIGNUM_ERR_MPI_NEGATIVE_VALUE;
        goto cleanup;
    }

    BIGNUM_MPI_CHK( Bignum_grow( X, A->n ) );

    /* Set the high limbs of X to match A. Don't touch the lower limbs
     * because X might be aliased to B, and we must not overwrite the
     * significant digits of B. */
    if( A->n > n )
        memcpy( X->p + n, A->p + n, ( A->n - n ) * ciL );
    if( X->n > A->n )
        memset( X->p + A->n, 0, ( X->n - A->n ) * ciL );

    carry = mpi_sub_hlp( n, X->p, A->p, B->p );
    if( carry != 0 )
    {
        /* Propagate the carry to the first nonzero limb of X. */
        for( ; n < X->n && X->p[n] == 0; n++ )
            --X->p[n];
        /* If we ran out of space for the carry, it means that the result
         * is negative. */
        if( n == X->n )
        {
            ret = BIGNUM_ERR_MPI_NEGATIVE_VALUE;
            goto cleanup;
        }
        --X->p[n];
    }

    /* X should always be positive as a result of unsigned subtractions. */
    X->s = 1;

cleanup:
    return( ret );
}

/*
 * Signed addition: X = A + B
 */
int Bignum_add_mpi( Bignum *X, const Bignum *A, const Bignum *B )
{
    int s, ret = 0;
    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( A != NULL );
    MPI_VALIDATE_RET( B != NULL );

    s = A->s;
    if( A->s * B->s < 0 )
    {
        if( Bignum_cmp_abs( A, B ) >= 0 )
        {
            BIGNUM_MPI_CHK( Bignum_sub_abs( X, A, B ) );
            X->s =  s;
        }
        else
        {
            BIGNUM_MPI_CHK( Bignum_sub_abs( X, B, A ) );
            X->s = -s;
        }
    }
    else
    {
        BIGNUM_MPI_CHK( Bignum_add_abs( X, A, B ) );
        X->s = s;
    }

cleanup:

    return( ret );
}

/*
 * Signed subtraction: X = A - B
 */
int Bignum_sub_mpi( Bignum *X, const Bignum *A, const Bignum *B )
{
    int ret = 0, s;
    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( A != NULL );
    MPI_VALIDATE_RET( B != NULL );

    s = A->s;
    if( A->s * B->s > 0 )
    {
        if( Bignum_cmp_abs( A, B ) >= 0 )
        {
            BIGNUM_MPI_CHK( Bignum_sub_abs( X, A, B ) );
            X->s =  s;
        }
        else
        {
            BIGNUM_MPI_CHK( Bignum_sub_abs( X, B, A ) );
            X->s = -s;
        }
    }
    else
    {
        BIGNUM_MPI_CHK( Bignum_add_abs( X, A, B ) );
        X->s = s;
    }

cleanup:

    return( ret );
}

/*
 * Signed addition: X = A + b
 */
int Bignum_add_int( Bignum *X, const Bignum *A, intt b )
{
    Bignum B;
    uintt p[1];
    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( A != NULL );

    p[0] = ( b < 0 ) ? -b : b;
    B.s = ( b < 0 ) ? -1 : 1;
    B.n = 1;
    B.p = p;

    return( Bignum_add_mpi( X, A, &B ) );
}

/*
 * Signed subtraction: X = A - b
 */
int Bignum_sub_int( Bignum *X, const Bignum *A, intt b )
{
    Bignum B;
    uintt p[1];
    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( A != NULL );

    p[0] = ( b < 0 ) ? -b : b;
    B.s = ( b < 0 ) ? -1 : 1;
    B.n = 1;
    B.p = p;

    return( Bignum_sub_mpi( X, A, &B ) );
}

/** Helper for Bignum multiplication.
 *
 * Add \p b * \p s to \p d.
 *
 * \param i             The number of limbs of \p s.
 * \param[in] s         A bignum to multiply, of size \p i.
 *                      It may overlap with \p d, but only if
 *                      \p d <= \p s.
 *                      Its leading limb must not be \c 0.
 * \param[in,out] d     The bignum to add to.
 *                      It must be sufficiently large to store the
 *                      result of the multiplication. This means
 *                      \p i + 1 limbs if \p d[\p i - 1] started as 0 and \p b
 *                      is not known a priori.
 * \param b             A scalar to multiply.
 */
static
#if defined(__APPLE__) && defined(__arm__)
/*
 * Apple LLVM version 4.2 (clang-425.0.24) (based on LLVM 3.2svn)
 * appears to need this to prevent bad ARM code generation at -O3.
 */
__attribute__ ((noinline))
#endif
void mpi_mul_hlp( size_t i,
                  const uintt *s,
                  uintt *d,
                  uintt b )
{
    uintt c = 0, t = 0;

#if defined(MULADDC_HUIT)
    for( ; i >= 8; i -= 8 )
    {
        MULADDC_INIT
        MULADDC_HUIT
        MULADDC_STOP
    }

    for( ; i > 0; i-- )
    {
        MULADDC_INIT
        MULADDC_CORE
        MULADDC_STOP
    }
#else /* MULADDC_HUIT */
    for( ; i >= 16; i -= 16 )
    {
        MULADDC_INIT
        MULADDC_CORE   MULADDC_CORE
        MULADDC_CORE   MULADDC_CORE
        MULADDC_CORE   MULADDC_CORE
        MULADDC_CORE   MULADDC_CORE

        MULADDC_CORE   MULADDC_CORE
        MULADDC_CORE   MULADDC_CORE
        MULADDC_CORE   MULADDC_CORE
        MULADDC_CORE   MULADDC_CORE
        MULADDC_STOP
    }

    for( ; i >= 8; i -= 8 )
    {
        MULADDC_INIT
        MULADDC_CORE   MULADDC_CORE
        MULADDC_CORE   MULADDC_CORE

        MULADDC_CORE   MULADDC_CORE
        MULADDC_CORE   MULADDC_CORE
        MULADDC_STOP
    }

    for( ; i > 0; i-- )
    {
        MULADDC_INIT
        MULADDC_CORE
        MULADDC_STOP
    }
#endif /* MULADDC_HUIT */

    t++;

    while( c != 0 )
    {
        *d += c; c = ( *d < c ); d++;
    }
}

/*
 * Baseline multiplication: X = A * B  (HAC 14.12)
 */
int Bignum_mul_mpi( Bignum *X, const Bignum *A, const Bignum *B )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    size_t i, j;
    Bignum TA, TB;
    int result_is_zero = 0;
    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( A != NULL );
    MPI_VALIDATE_RET( B != NULL );

    Bignum_init( &TA ); Bignum_init( &TB );

    if( X == A ) { BIGNUM_MPI_CHK( Bignum_copy( &TA, A ) ); A = &TA; }
    if( X == B ) { BIGNUM_MPI_CHK( Bignum_copy( &TB, B ) ); B = &TB; }

    for( i = A->n; i > 0; i-- )
        if( A->p[i - 1] != 0 )
            break;
    if( i == 0 )
        result_is_zero = 1;

    for( j = B->n; j > 0; j-- )
        if( B->p[j - 1] != 0 )
            break;
    if( j == 0 )
        result_is_zero = 1;

    BIGNUM_MPI_CHK( Bignum_grow( X, i + j ) );
    BIGNUM_MPI_CHK( Bignum_lset( X, 0 ) );

    for( ; j > 0; j-- )
        mpi_mul_hlp( i, A->p, X->p + j - 1, B->p[j - 1] );

    /* If the result is 0, we don't shortcut the operation, which reduces
     * but does not eliminate side channels leaking the zero-ness. We do
     * need to take care to set the sign bit properly since the library does
     * not fully support an MPI object with a value of 0 and s == -1. */
    if( result_is_zero )
        X->s = 1;
    else
        X->s = A->s * B->s;

cleanup:

    Bignum_free( &TB ); Bignum_free( &TA );

    return( ret );
}

/*
 * Baseline multiplication: X = A * b
 */
int Bignum_mul_int( Bignum *X, const Bignum *A, uintt b )
{
    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( A != NULL );

    /* mpi_mul_hlp can't deal with a leading 0. */
    size_t n = A->n;
    while( n > 0 && A->p[n - 1] == 0 )
        --n;

    /* The general method below doesn't work if n==0 or b==0. By chance
     * calculating the result is trivial in those cases. */
    if( b == 0 || n == 0 )
    {
        return( Bignum_lset( X, 0 ) );
    }

    /* Calculate A*b as A + A*(b-1) to take advantage of mpi_mul_hlp */
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    /* In general, A * b requires 1 limb more than b. If
     * A->p[n - 1] * b / b == A->p[n - 1], then A * b fits in the same
     * number of limbs as A and the call to grow() is not required since
     * copy() will take care of the growth if needed. However, experimentally,
     * making the call to grow() unconditional causes slightly fewer
     * calls to calloc() in ECP code, presumably because it reuses the
     * same mpi for a while and this way the mpi is more likely to directly
     * grow to its final size. */
    BIGNUM_MPI_CHK( Bignum_grow( X, n + 1 ) );
    BIGNUM_MPI_CHK( Bignum_copy( X, A ) );
    mpi_mul_hlp( n, A->p, X->p, b - 1 );

cleanup:
    return( ret );
}

/*
 * Unsigned integer divide - double uintt dividend, u1/u0, and
 * uintt divisor, d
 */
static uintt mbedtls_int_div_int( uintt u1,
            uintt u0, uintt d, uintt *r )
{
#if defined(BIGNUM_HAVE_UDBL)
    udbl dividend, quotient;
#else
    const uintt radix = (uintt) 1 << biH;
    const uintt uint_halfword_mask = ( (uintt) 1 << biH ) - 1;
    uintt d0, d1, q0, q1, rAX, r0, quotient;
    uintt u0_msw, u0_lsw;
    size_t s;
#endif

    /*
     * Check for overflow
     */
    if( 0 == d || u1 >= d )
    {
        if (r != NULL) *r = ~0;

        return ( ~0 );
    }

#if defined(BIGNUM_HAVE_UDBL)
    dividend  = (udbl) u1 << biL;
    dividend |= (udbl) u0;
    quotient = dividend / d;
    if( quotient > ( (udbl) 1 << biL ) - 1 )
        quotient = ( (udbl) 1 << biL ) - 1;

    if( r != NULL )
        *r = (uintt)( dividend - (quotient * d ) );

    return (uintt) quotient;
#else

    /*
     * Algorithm D, Section 4.3.1 - The Art of Computer Programming
     *   Vol. 2 - Seminumerical Algorithms, Knuth
     */

    /*
     * Normalize the divisor, d, and dividend, u0, u1
     */
    s = mbedtls_clz( d );
    d = d << s;

    u1 = u1 << s;
    u1 |= ( u0 >> ( biL - s ) ) & ( -(intt)s >> ( biL - 1 ) );
    u0 =  u0 << s;

    d1 = d >> biH;
    d0 = d & uint_halfword_mask;

    u0_msw = u0 >> biH;
    u0_lsw = u0 & uint_halfword_mask;

    /*
     * Find the first quotient and remainder
     */
    q1 = u1 / d1;
    r0 = u1 - d1 * q1;

    while( q1 >= radix || ( q1 * d0 > radix * r0 + u0_msw ) )
    {
        q1 -= 1;
        r0 += d1;

        if ( r0 >= radix ) break;
    }

    rAX = ( u1 * radix ) + ( u0_msw - q1 * d );
    q0 = rAX / d1;
    r0 = rAX - q0 * d1;

    while( q0 >= radix || ( q0 * d0 > radix * r0 + u0_lsw ) )
    {
        q0 -= 1;
        r0 += d1;

        if ( r0 >= radix ) break;
    }

    if (r != NULL)
        *r = ( rAX * radix + u0_lsw - q0 * d ) >> s;

    quotient = q1 * radix + q0;

    return quotient;
#endif
}

/*
 * Division by Bignum: A = Q * B + R  (HAC 14.20)
 */
int Bignum_div_mpi( Bignum *Q, Bignum *R, const Bignum *A,
                         const Bignum *B )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    size_t i, n, t, k;
    Bignum X, Y, Z, T1, T2;
    uintt TP2[3];
    MPI_VALIDATE_RET( A != NULL );
    MPI_VALIDATE_RET( B != NULL );

    if( Bignum_cmp_int( B, 0 ) == 0 )
        return( BIGNUM_ERR_MPI_DIVISION_BY_ZERO );

    Bignum_init( &X ); Bignum_init( &Y ); Bignum_init( &Z );
    Bignum_init( &T1 );
    /*
     * Avoid dynamic memory allocations for constant-size T2.
     *
     * T2 is used for comparison only and the 3 limbs are assigned explicitly,
     * so nobody increase the size of the MPI and we're safe to use an on-stack
     * buffer.
     */
    T2.s = 1;
    T2.n = sizeof( TP2 ) / sizeof( *TP2 );
    T2.p = TP2;

    if( Bignum_cmp_abs( A, B ) < 0 )
    {
        if( Q != NULL ) BIGNUM_MPI_CHK( Bignum_lset( Q, 0 ) );
        if( R != NULL ) BIGNUM_MPI_CHK( Bignum_copy( R, A ) );
        return( 0 );
    }

    BIGNUM_MPI_CHK( Bignum_copy( &X, A ) );
    BIGNUM_MPI_CHK( Bignum_copy( &Y, B ) );
    X.s = Y.s = 1;

    BIGNUM_MPI_CHK( Bignum_grow( &Z, A->n + 2 ) );
    BIGNUM_MPI_CHK( Bignum_lset( &Z,  0 ) );
    BIGNUM_MPI_CHK( Bignum_grow( &T1, A->n + 2 ) );

    k = Bignum_bitlen( &Y ) % biL;
    if( k < biL - 1 )
    {
        k = biL - 1 - k;
        BIGNUM_MPI_CHK( Bignum_shift_l( &X, k ) );
        BIGNUM_MPI_CHK( Bignum_shift_l( &Y, k ) );
    }
    else k = 0;

    n = X.n - 1;
    t = Y.n - 1;
    BIGNUM_MPI_CHK( Bignum_shift_l( &Y, biL * ( n - t ) ) );

    while( Bignum_cmp_mpi( &X, &Y ) >= 0 )
    {
        Z.p[n - t]++;
        BIGNUM_MPI_CHK( Bignum_sub_mpi( &X, &X, &Y ) );
    }
    BIGNUM_MPI_CHK( Bignum_shift_r( &Y, biL * ( n - t ) ) );

    for( i = n; i > t ; i-- )
    {
        if( X.p[i] >= Y.p[t] )
            Z.p[i - t - 1] = ~0;
        else
        {
            Z.p[i - t - 1] = mbedtls_int_div_int( X.p[i], X.p[i - 1],
                                                            Y.p[t], NULL);
        }

        T2.p[0] = ( i < 2 ) ? 0 : X.p[i - 2];
        T2.p[1] = ( i < 1 ) ? 0 : X.p[i - 1];
        T2.p[2] = X.p[i];

        Z.p[i - t - 1]++;
        do
        {
            Z.p[i - t - 1]--;

            BIGNUM_MPI_CHK( Bignum_lset( &T1, 0 ) );
            T1.p[0] = ( t < 1 ) ? 0 : Y.p[t - 1];
            T1.p[1] = Y.p[t];
            BIGNUM_MPI_CHK( Bignum_mul_int( &T1, &T1, Z.p[i - t - 1] ) );
        }
        while( Bignum_cmp_mpi( &T1, &T2 ) > 0 );

        BIGNUM_MPI_CHK( Bignum_mul_int( &T1, &Y, Z.p[i - t - 1] ) );
        BIGNUM_MPI_CHK( Bignum_shift_l( &T1,  biL * ( i - t - 1 ) ) );
        BIGNUM_MPI_CHK( Bignum_sub_mpi( &X, &X, &T1 ) );

        if( Bignum_cmp_int( &X, 0 ) < 0 )
        {
            BIGNUM_MPI_CHK( Bignum_copy( &T1, &Y ) );
            BIGNUM_MPI_CHK( Bignum_shift_l( &T1, biL * ( i - t - 1 ) ) );
            BIGNUM_MPI_CHK( Bignum_add_mpi( &X, &X, &T1 ) );
            Z.p[i - t - 1]--;
        }
    }

    if( Q != NULL )
    {
        BIGNUM_MPI_CHK( Bignum_copy( Q, &Z ) );
        Q->s = A->s * B->s;
    }

    if( R != NULL )
    {
        BIGNUM_MPI_CHK( Bignum_shift_r( &X, k ) );
        X.s = A->s;
        BIGNUM_MPI_CHK( Bignum_copy( R, &X ) );

        if( Bignum_cmp_int( R, 0 ) == 0 )
            R->s = 1;
    }

cleanup:

    Bignum_free( &X ); Bignum_free( &Y ); Bignum_free( &Z );
    Bignum_free( &T1 );
    memset(TP2, 0, sizeof( TP2 ));

    return( ret );
}

/*
 * Division by int: A = Q * b + R
 */
int Bignum_div_int( Bignum *Q, Bignum *R, const Bignum *A, intt b ){
    Bignum B;
    uintt p[1];
    MPI_VALIDATE_RET( A != NULL );

    p[0] = ( b < 0 ) ? -b : b;
    B.s = ( b < 0 ) ? -1 : 1;
    B.n = 1;
    B.p = p;

    return( Bignum_div_mpi( Q, R, A, &B ) );
}

/*
 * Modulo: R = A mod B
 */
int Bignum_mod_mpi( Bignum *R, const Bignum *A, const Bignum *B )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    MPI_VALIDATE_RET( R != NULL );
    MPI_VALIDATE_RET( A != NULL );
    MPI_VALIDATE_RET( B != NULL );

    if( Bignum_cmp_int( B, 0 ) < 0 )
        return( BIGNUM_ERR_MPI_NEGATIVE_VALUE );

    BIGNUM_MPI_CHK( Bignum_div_mpi( NULL, R, A, B ) );

    while( Bignum_cmp_int( R, 0 ) < 0 )
      BIGNUM_MPI_CHK( Bignum_add_mpi( R, R, B ) );

    while( Bignum_cmp_mpi( R, B ) >= 0 )
      BIGNUM_MPI_CHK( Bignum_sub_mpi( R, R, B ) );

cleanup:

    return( ret );
}

/*
 * Modulo: r = A mod b
 */
int Bignum_mod_int( uintt *r, const Bignum *A, intt b )
{
    size_t i;
    uintt x, y, z;
    MPI_VALIDATE_RET( r != NULL );
    MPI_VALIDATE_RET( A != NULL );

    if( b == 0 )
        return( BIGNUM_ERR_MPI_DIVISION_BY_ZERO );

    if( b < 0 )
        return( BIGNUM_ERR_MPI_NEGATIVE_VALUE );

    /*
     * handle trivial cases
     */
    if( b == 1 )
    {
        *r = 0;
        return( 0 );
    }

    if( b == 2 )
    {
        *r = A->p[0] & 1;
        return( 0 );
    }

    /*
     * general case
     */
    for( i = A->n, y = 0; i > 0; i-- )
    {
        x  = A->p[i - 1];
        y  = ( y << biH ) | ( x >> biH );
        z  = y / b;
        y -= z * b;

        x <<= biH;
        y  = ( y << biH ) | ( x >> biH );
        z  = y / b;
        y -= z * b;
    }

    /*
     * If A is negative, then the current y represents a negative value.
     * Flipping it to the positive side.
     */
    if( A->s < 0 && y != 0 )
        y = b - y;

    *r = y;

    return( 0 );
}

/*
 * Fast Montgomery initialization (thanks to Tom St Denis)
 */
static void mpi_montg_init( uintt *mm, const Bignum *N )
{
    uintt x, m0 = N->p[0];
    unsigned int i;

    x  = m0;
    x += ( ( m0 + 2 ) & 4 ) << 1;

    for( i = biL; i >= 8; i /= 2 )
        x *= ( 2 - ( m0 * x ) );

    *mm = ~x + 1;
}

/** Montgomery multiplication: A = A * B * R^-1 mod N  (HAC 14.36)
 *
 * \param[in,out]   A   One of the numbers to multiply.
 *                      It must have at least as many limbs as N
 *                      (A->n >= N->n), and any limbs beyond n are ignored.
 *                      On successful completion, A contains the result of
 *                      the multiplication A * B * R^-1 mod N where
 *                      R = (2^ciL)^n.
 * \param[in]       B   One of the numbers to multiply.
 *                      It must be nonzero and must not have more limbs than N
 *                      (B->n <= N->n).
 * \param[in]       N   The modulo. N must be odd.
 * \param           mm  The value calculated by `mpi_montg_init(&mm, N)`.
 *                      This is -N^-1 mod 2^ciL.
 * \param[in,out]   T   A bignum for temporary storage.
 *                      It must be at least twice the limb size of N plus 2
 *                      (T->n >= 2 * (N->n + 1)).
 *                      Its initial content is unused and
 *                      its final content is indeterminate.
 *                      Note that unlike the usual convention in the library
 *                      for `const Bignum*`, the content of T can change.
 */
static void mpi_montmul( Bignum *A, const Bignum *B, const Bignum *N, uintt mm,
                         const Bignum *T )
{
    size_t i, n, m;
    uintt u0, u1, *d;

    memset( T->p, 0, T->n * ciL );

    d = T->p;
    n = N->n;
    m = ( B->n < n ) ? B->n : n;

    for( i = 0; i < n; i++ )
    {
        /*
         * T = (T + u0*B + u1*N) / 2^biL
         */
        u0 = A->p[i];
        u1 = ( d[0] + u0 * B->p[0] ) * mm;

        mpi_mul_hlp( m, B->p, d, u0 );
        mpi_mul_hlp( n, N->p, d, u1 );

        *d++ = u0; d[n + 1] = 0;
    }

    /* At this point, d is either the desired result or the desired result
     * plus N. We now potentially subtract N, avoiding leaking whether the
     * subtraction is performed through side channels. */

    /* Copy the n least significant limbs of d to A, so that
     * A = d if d < N (recall that N has n limbs). */
    memcpy( A->p, d, n * ciL );
    /* If d >= N then we want to set A to d - N. To prevent timing attacks,
     * do the calculation without using conditional tests. */
    /* Set d to d0 + (2^biL)^n - N where d0 is the current value of d. */
    d[n] += 1;
    d[n] -= mpi_sub_hlp( n, d, d, N->p );
    /* If d0 < N then d < (2^biL)^n
     * so d[n] == 0 and we want to keep A as it is.
     * If d0 >= N then d >= (2^biL)^n, and d <= (2^biL)^n + N < 2 * (2^biL)^n
     * so d[n] == 1 and we want to set A to the result of the subtraction
     * which is d - (2^biL)^n, i.e. the n least significant limbs of d.
     * This exactly corresponds to a conditional assignment. */
    mpi_safe_cond_assign( n, A->p, d, (unsigned char) d[n] );
}

/*
 * Montgomery reduction: A = A * R^-1 mod N
 *
 * See mpi_montmul() regarding constraints and guarantees on the parameters.
 */
static void mpi_montred( Bignum *A, const Bignum *N, uintt mm, const Bignum *T ){
    uintt z = 1;
    Bignum U;

    U.n = U.s = (int) z;
    U.p = &z;

    mpi_montmul( A, &U, N, mm, T );
}

/*
 * Constant-flow boolean "equal" comparison:
 * return x == y
 *
 * This function can be used to write constant-time code by replacing branches
 * with bit operations - it can be used in conjunction with
 * mbedtls_ssl_cf_mask_from_bit().
 *
 * This function is implemented without using comparison operators, as those
 * might be translated to branches by some compilers on some platforms.
 */
static size_t Bignum_cf_bool_eq( size_t x, size_t y )
{
    /* diff = 0 if x == y, non-zero otherwise */
    const size_t diff = x ^ y;

    /* MSVC has a warning about unary minus on unsigned integer types,
     * but this is well-defined and precisely what we want to do here. */
#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4146 )
#endif

    /* diff_msb's most significant bit is equal to x != y */
    const size_t diff_msb = ( diff | (size_t) -diff );

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

    /* diff1 = (x != y) ? 1 : 0 */
    const size_t diff1 = diff_msb >> ( sizeof( diff_msb ) * 8 - 1 );

    return( 1 ^ diff1 );
}

/**
 * Select an MPI from a table without leaking the index.
 *
 * This is functionally equivalent to Bignum_copy(R, T[idx]) except it
 * reads the entire table in order to avoid leaking the value of idx to an
 * attacker able to observe memory access patterns.
 *
 * \param[out] R        Where to write the selected MPI.
 * \param[in] T         The table to read from.
 * \param[in] T_size    The number of elements in the table.
 * \param[in] idx       The index of the element to select;
 *                      this must satisfy 0 <= idx < T_size.
 *
 * \return \c 0 on success, or a negative error code.
 */
static int mpi_select( Bignum *R, const Bignum *T, size_t T_size, size_t idx )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;

    for( size_t i = 0; i < T_size; i++ )
    {
        BIGNUM_MPI_CHK( Bignum_safe_cond_assign( R, &T[i],
                        (unsigned char) Bignum_cf_bool_eq( i, idx ) ) );
    }

cleanup:
    return( ret );
}

/*
 * Sliding-window exponentiation: X = A^E mod N  (HAC 14.85)
 */
int Bignum_exp_mod( Bignum *X, const Bignum *A, const Bignum *E,
                    const Bignum *N, Bignum *prec_RR )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    size_t wbits, wsize, one = 1;
    size_t i, j, nblimbs;
    size_t bufsize, nbits;
    uintt ei, mm, state;
    Bignum RR, T, W[ 1 << BIGNUM_MPI_WINDOW_SIZE ], WW, Apos;
    int neg;

    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( A != NULL );
    MPI_VALIDATE_RET( E != NULL );
    MPI_VALIDATE_RET( N != NULL );

    if( Bignum_cmp_int( N, 0 ) <= 0 || ( N->p[0] & 1 ) == 0 )
        return( BIGNUM_ERR_MPI_BAD_INPUT_DATA );

    if( Bignum_cmp_int( E, 0 ) < 0 )
        return( BIGNUM_ERR_MPI_BAD_INPUT_DATA );

    if( Bignum_bitlen( E ) > BIGNUM_MPI_MAX_BITS ||
        Bignum_bitlen( N ) > BIGNUM_MPI_MAX_BITS )
        return ( BIGNUM_ERR_MPI_BAD_INPUT_DATA );

    /*
     * Init temps and window size
     */
    mpi_montg_init( &mm, N );
    Bignum_init( &RR ); Bignum_init( &T );
    Bignum_init( &Apos );
    Bignum_init( &WW );
    memset( W, 0, sizeof( W ) );

    i = Bignum_bitlen( E );

    wsize = ( i > 671 ) ? 6 : ( i > 239 ) ? 5 :
            ( i >  79 ) ? 4 : ( i >  23 ) ? 3 : 1;

#if( BIGNUM_MPI_WINDOW_SIZE < 6 )
    if( wsize > BIGNUM_MPI_WINDOW_SIZE )
        wsize = BIGNUM_MPI_WINDOW_SIZE;
#endif

    j = N->n + 1;
    /* All W[i] and X must have at least N->n limbs for the mpi_montmul()
     * and mpi_montred() calls later. Here we ensure that W[1] and X are
     * large enough, and later we'll grow other W[i] to the same length.
     * They must not be shrunk midway through this function!
     */
    BIGNUM_MPI_CHK( Bignum_grow( X, j ) );
    BIGNUM_MPI_CHK( Bignum_grow( &W[1],  j ) );
    BIGNUM_MPI_CHK( Bignum_grow( &T, j * 2 ) );

    /*
     * Compensate for negative A (and correct at the end)
     */
    neg = ( A->s == -1 );
    if( neg )
    {
        BIGNUM_MPI_CHK( Bignum_copy( &Apos, A ) );
        Apos.s = 1;
        A = &Apos;
    }

    /*
     * If 1st call, pre-compute R^2 mod N
     */
    if( prec_RR == NULL || prec_RR->p == NULL )
    {
        BIGNUM_MPI_CHK( Bignum_lset( &RR, 1 ) );
        BIGNUM_MPI_CHK( Bignum_shift_l( &RR, N->n * 2 * biL ) );
        BIGNUM_MPI_CHK( Bignum_mod_mpi( &RR, &RR, N ) );

        if( prec_RR != NULL )
            memcpy( prec_RR, &RR, sizeof( Bignum ) );
    }
    else
        memcpy( &RR, prec_RR, sizeof( Bignum ) );

    /*
     * W[1] = A * R^2 * R^-1 mod N = A * R mod N
     */
    if( Bignum_cmp_mpi( A, N ) >= 0 )
    {
        BIGNUM_MPI_CHK( Bignum_mod_mpi( &W[1], A, N ) );
        /* This should be a no-op because W[1] is already that large before
         * Bignum_mod_mpi(), but it's necessary to avoid an overflow
         * in mpi_montmul() below, so let's make sure. */
        BIGNUM_MPI_CHK( Bignum_grow( &W[1], N->n + 1 ) );
    }
    else
        BIGNUM_MPI_CHK( Bignum_copy( &W[1], A ) );

    /* Note that this is safe because W[1] always has at least N->n limbs
     * (it grew above and was preserved by Bignum_copy()). */
    mpi_montmul( &W[1], &RR, N, mm, &T );

    /*
     * X = R^2 * R^-1 mod N = R mod N
     */
    BIGNUM_MPI_CHK( Bignum_copy( X, &RR ) );
    mpi_montred( X, N, mm, &T );

    if( wsize > 1 )
    {
        /*
         * W[1 << (wsize - 1)] = W[1] ^ (wsize - 1)
         */
        j =  one << ( wsize - 1 );

        BIGNUM_MPI_CHK( Bignum_grow( &W[j], N->n + 1 ) );
        BIGNUM_MPI_CHK( Bignum_copy( &W[j], &W[1]    ) );

        for( i = 0; i < wsize - 1; i++ )
            mpi_montmul( &W[j], &W[j], N, mm, &T );

        /*
         * W[i] = W[i - 1] * W[1]
         */
        for( i = j + 1; i < ( one << wsize ); i++ )
        {
            BIGNUM_MPI_CHK( Bignum_grow( &W[i], N->n + 1 ) );
            BIGNUM_MPI_CHK( Bignum_copy( &W[i], &W[i - 1] ) );

            mpi_montmul( &W[i], &W[1], N, mm, &T );
        }
    }

    nblimbs = E->n;
    bufsize = 0;
    nbits   = 0;
    wbits   = 0;
    state   = 0;

    while( 1 )
    {
        if( bufsize == 0 )
        {
            if( nblimbs == 0 )
                break;

            nblimbs--;

            bufsize = sizeof( uintt ) << 3;
        }

        bufsize--;

        ei = (E->p[nblimbs] >> bufsize) & 1;

        /*
         * skip leading 0s
         */
        if( ei == 0 && state == 0 )
            continue;

        if( ei == 0 && state == 1 )
        {
            /*
             * out of window, square X
             */
            mpi_montmul( X, X, N, mm, &T );
            continue;
        }

        /*
         * add ei to current window
         */
        state = 2;

        nbits++;
        wbits |= ( ei << ( wsize - nbits ) );

        if( nbits == wsize )
        {
            /*
             * X = X^wsize R^-1 mod N
             */
            for( i = 0; i < wsize; i++ )
                mpi_montmul( X, X, N, mm, &T );

            /*
             * X = X * W[wbits] R^-1 mod N
             */
            BIGNUM_MPI_CHK( mpi_select( &WW, W, (size_t) 1 << wsize, wbits ) );
            mpi_montmul( X, &WW, N, mm, &T );

            state--;
            nbits = 0;
            wbits = 0;
        }
    }

    /*
     * process the remaining bits
     */
    for( i = 0; i < nbits; i++ )
    {
        mpi_montmul( X, X, N, mm, &T );

        wbits <<= 1;

        if( ( wbits & ( one << wsize ) ) != 0 )
            mpi_montmul( X, &W[1], N, mm, &T );
    }

    /*
     * X = A^E * R * R^-1 mod N = A^E mod N
     */
    mpi_montred( X, N, mm, &T );

    if( neg && E->n != 0 && ( E->p[0] & 1 ) != 0 )
    {
        X->s = -1;
        BIGNUM_MPI_CHK( Bignum_add_mpi( X, N, X ) );
    }

cleanup:

    for( i = ( one << ( wsize - 1 ) ); i < ( one << wsize ); i++ )
        Bignum_free( &W[i] );

    Bignum_free( &W[1] ); Bignum_free( &T ); Bignum_free( &Apos );
    Bignum_free( &WW );

    if( prec_RR == NULL || prec_RR->p == NULL )
        Bignum_free( &RR );

    return( ret );
}

/*
 * Greatest common divisor: G = gcd(A, B)  (HAC 14.54)
 */
int Bignum_gcd( Bignum *G, const Bignum *A, const Bignum *B )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    size_t lz, lzt;
    Bignum TA, TB;

    MPI_VALIDATE_RET( G != NULL );
    MPI_VALIDATE_RET( A != NULL );
    MPI_VALIDATE_RET( B != NULL );

    Bignum_init( &TA ); Bignum_init( &TB );

    BIGNUM_MPI_CHK( Bignum_copy( &TA, A ) );
    BIGNUM_MPI_CHK( Bignum_copy( &TB, B ) );

    lz = Bignum_lsb( &TA );
    lzt = Bignum_lsb( &TB );

    /* The loop below gives the correct result when A==0 but not when B==0.
     * So have a special case for B==0. Leverage the fact that we just
     * calculated the lsb and lsb(B)==0 iff B is odd or 0 to make the test
     * slightly more efficient than cmp_int(). */
    if( lzt == 0 && Bignum_get_bit( &TB, 0 ) == 0 )
    {
        ret = Bignum_copy( G, A );
        goto cleanup;
    }

    if( lzt < lz )
        lz = lzt;

    TA.s = TB.s = 1;

    /* We mostly follow the procedure described in HAC 14.54, but with some
     * minor differences:
     * - Sequences of multiplications or divisions by 2 are grouped into a
     *   single shift operation.
     * - The procedure in HAC assumes that 0 < TB <= TA.
     *     - The condition TB <= TA is not actually necessary for correctness.
     *       TA and TB have symmetric roles except for the loop termination
     *       condition, and the shifts at the beginning of the loop body
     *       remove any significance from the ordering of TA vs TB before
     *       the shifts.
     *     - If TA = 0, the loop goes through 0 iterations and the result is
     *       correctly TB.
     *     - The case TB = 0 was short-circuited above.
     *
     * For the correctness proof below, decompose the original values of
     * A and B as
     *   A = sa * 2^a * A' with A'=0 or A' odd, and sa = +-1
     *   B = sb * 2^b * B' with B'=0 or B' odd, and sb = +-1
     * Then gcd(A, B) = 2^{min(a,b)} * gcd(A',B'),
     * and gcd(A',B') is odd or 0.
     *
     * At the beginning, we have TA = |A| and TB = |B| so gcd(A,B) = gcd(TA,TB).
     * The code maintains the following invariant:
     *     gcd(A,B) = 2^k * gcd(TA,TB) for some k   (I)
     */

    /* Proof that the loop terminates:
     * At each iteration, either the right-shift by 1 is made on a nonzero
     * value and the nonnegative integer bitlen(TA) + bitlen(TB) decreases
     * by at least 1, or the right-shift by 1 is made on zero and then
     * TA becomes 0 which ends the loop (TB cannot be 0 if it is right-shifted
     * since in that case TB is calculated from TB-TA with the condition TB>TA).
     */
    while( Bignum_cmp_int( &TA, 0 ) != 0 )
    {
        /* Divisions by 2 preserve the invariant (I). */
        BIGNUM_MPI_CHK( Bignum_shift_r( &TA, Bignum_lsb( &TA ) ) );
        BIGNUM_MPI_CHK( Bignum_shift_r( &TB, Bignum_lsb( &TB ) ) );

        /* Set either TA or TB to |TA-TB|/2. Since TA and TB are both odd,
         * TA-TB is even so the division by 2 has an integer result.
         * Invariant (I) is preserved since any odd divisor of both TA and TB
         * also divides |TA-TB|/2, and any odd divisor of both TA and |TA-TB|/2
         * also divides TB, and any odd divisior of both TB and |TA-TB|/2 also
         * divides TA.
         */
        if( Bignum_cmp_mpi( &TA, &TB ) >= 0 )
        {
            BIGNUM_MPI_CHK( Bignum_sub_abs( &TA, &TA, &TB ) );
            BIGNUM_MPI_CHK( Bignum_shift_r( &TA, 1 ) );
        }
        else
        {
            BIGNUM_MPI_CHK( Bignum_sub_abs( &TB, &TB, &TA ) );
            BIGNUM_MPI_CHK( Bignum_shift_r( &TB, 1 ) );
        }
        /* Note that one of TA or TB is still odd. */
    }

    /* By invariant (I), gcd(A,B) = 2^k * gcd(TA,TB) for some k.
     * At the loop exit, TA = 0, so gcd(TA,TB) = TB.
     * - If there was at least one loop iteration, then one of TA or TB is odd,
     *   and TA = 0, so TB is odd and gcd(TA,TB) = gcd(A',B'). In this case,
     *   lz = min(a,b) so gcd(A,B) = 2^lz * TB.
     * - If there was no loop iteration, then A was 0, and gcd(A,B) = B.
     *   In this case, lz = 0 and B = TB so gcd(A,B) = B = 2^lz * TB as well.
     */

    BIGNUM_MPI_CHK( Bignum_shift_l( &TB, lz ) );
    BIGNUM_MPI_CHK( Bignum_copy( G, &TB ) );

cleanup:

    Bignum_free( &TA ); Bignum_free( &TB );

    return( ret );
}

/* Fill X with n_bytes random bytes.
 * X must already have room for those bytes.
 * The ordering of the bytes returned from the RNG is suitable for
 * deterministic ECDSA (see RFC 6979 §3.3 and Bignum_random()).
 * The size and sign of X are unchanged.
 * n_bytes must not be 0.
 */
static int mpi_fill_random_internal( Bignum *X, size_t n_bytes,
    int (*f_rng)(void *, unsigned char *, size_t), void *p_rng )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    const size_t limbs = CHARS_TO_LIMBS( n_bytes );
    const size_t overhead = ( limbs * ciL ) - n_bytes;

    if( X->n < limbs )
        return( BIGNUM_ERR_MPI_BAD_INPUT_DATA );

    memset( X->p, 0, overhead );
    memset( (unsigned char *) X->p + limbs * ciL, 0, ( X->n - limbs ) * ciL );
    BIGNUM_MPI_CHK( f_rng( p_rng, (unsigned char *) X->p + overhead, n_bytes ) );
    mpi_bigendian_to_host( X->p, limbs );

cleanup:
    return( ret );
}

/*
 * Fill X with size bytes of random.
 *
 * Use a temporary bytes representation to make sure the result is the same
 * regardless of the platform endianness (useful when f_rng is actually
 * deterministic, eg for tests).
 */
int Bignum_fill_random( Bignum *X, size_t size,
                     int (*f_rng)(void *, unsigned char *, size_t),
                     void *p_rng )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    size_t const limbs = CHARS_TO_LIMBS( size );

    MPI_VALIDATE_RET( X     != NULL );
    MPI_VALIDATE_RET( f_rng != NULL );

    /* Ensure that target MPI has exactly the necessary number of limbs */
    BIGNUM_MPI_CHK( Bignum_resize_clear( X, limbs ) );
    if( size == 0 )
        return( 0 );

    ret = mpi_fill_random_internal( X, size, f_rng, p_rng );

cleanup:
    return( ret );
}

int Bignum_random( Bignum *X, intt min, const Bignum *N,
                   int (*f_rng)(void *, unsigned char *, size_t), void *p_rng )
{
    int ret = BIGNUM_ERR_MPI_BAD_INPUT_DATA;
    int count;
    unsigned lt_lower = 1, lt_upper = 0;
    size_t n_bits = Bignum_bitlen( N );
    size_t n_bytes = ( n_bits + 7 ) / 8;
    Bignum lower_bound;

    if( min < 0 )
        return( BIGNUM_ERR_MPI_BAD_INPUT_DATA );
    if( Bignum_cmp_int( N, min ) <= 0 )
        return( BIGNUM_ERR_MPI_BAD_INPUT_DATA );

    /*
     * When min == 0, each try has at worst a probability 1/2 of failing
     * (the msb has a probability 1/2 of being 0, and then the result will
     * be < N), so after 30 tries failure probability is a most 2**(-30).
     *
     * When N is just below a power of 2, as is the case when generating
     * a random scalar on most elliptic curves, 1 try is enough with
     * overwhelming probability. When N is just above a power of 2,
     * as when generating a random scalar on secp224k1, each try has
     * a probability of failing that is almost 1/2.
     *
     * The probabilities are almost the same if min is nonzero but negligible
     * compared to N. This is always the case when N is crypto-sized, but
     * it's convenient to support small N for testing purposes. When N
     * is small, use a higher repeat count, otherwise the probability of
     * failure is macroscopic.
     */
    count = ( n_bytes > 4 ? 30 : 250 );

    Bignum_init( &lower_bound );

    /* Ensure that target MPI has exactly the same number of limbs
     * as the upper bound, even if the upper bound has leading zeros.
     * This is necessary for the Bignum_lt_mpi_ct() check. */
    BIGNUM_MPI_CHK( Bignum_resize_clear( X, N->n ) );
    BIGNUM_MPI_CHK( Bignum_grow( &lower_bound, N->n ) );
    BIGNUM_MPI_CHK( Bignum_lset( &lower_bound, min ) );

    /*
     * Match the procedure given in RFC 6979 §3.3 (deterministic ECDSA)
     * when f_rng is a suitably parametrized instance of HMAC_DRBG:
     * - use the same byte ordering;
     * - keep the leftmost n_bits bits of the generated octet string;
     * - try until result is in the desired range.
     * This also avoids any bias, which is especially important for ECDSA.
     */
    do
    {
        BIGNUM_MPI_CHK( mpi_fill_random_internal( X, n_bytes, f_rng, p_rng ) );
        BIGNUM_MPI_CHK( Bignum_shift_r( X, 8 * n_bytes - n_bits ) );

        if( --count == 0 )
        {
            ret = BIGNUM_ERR_MPI_NOT_ACCEPTABLE;
            goto cleanup;
        }

        BIGNUM_MPI_CHK( Bignum_lt_mpi_ct( X, &lower_bound, &lt_lower ) );
        BIGNUM_MPI_CHK( Bignum_lt_mpi_ct( X, N, &lt_upper ) );
    }
    while( lt_lower != 0 || lt_upper == 0 );

cleanup:
    Bignum_free( &lower_bound );
    return( ret );
}

/*
 * Modular inverse: X = A^-1 mod N  (HAC 14.61 / 14.64)
 */
int Bignum_inv_mod( Bignum *X, const Bignum *A, const Bignum *N )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    Bignum G, TA, TU, U1, U2, TB, TV, V1, V2;
    MPI_VALIDATE_RET( X != NULL );
    MPI_VALIDATE_RET( A != NULL );
    MPI_VALIDATE_RET( N != NULL );

    if( Bignum_cmp_int( N, 1 ) <= 0 )
        return( BIGNUM_ERR_MPI_BAD_INPUT_DATA );

    Bignum_init( &TA ); Bignum_init( &TU ); Bignum_init( &U1 ); Bignum_init( &U2 );
    Bignum_init( &G ); Bignum_init( &TB ); Bignum_init( &TV );
    Bignum_init( &V1 ); Bignum_init( &V2 );

    BIGNUM_MPI_CHK( Bignum_gcd( &G, A, N ) );

    if( Bignum_cmp_int( &G, 1 ) != 0 )
    {
        ret = BIGNUM_ERR_MPI_NOT_ACCEPTABLE;
        goto cleanup;
    }

    BIGNUM_MPI_CHK( Bignum_mod_mpi( &TA, A, N ) );
    BIGNUM_MPI_CHK( Bignum_copy( &TU, &TA ) );
    BIGNUM_MPI_CHK( Bignum_copy( &TB, N ) );
    BIGNUM_MPI_CHK( Bignum_copy( &TV, N ) );

    BIGNUM_MPI_CHK( Bignum_lset( &U1, 1 ) );
    BIGNUM_MPI_CHK( Bignum_lset( &U2, 0 ) );
    BIGNUM_MPI_CHK( Bignum_lset( &V1, 0 ) );
    BIGNUM_MPI_CHK( Bignum_lset( &V2, 1 ) );

    do
    {
        while( ( TU.p[0] & 1 ) == 0 )
        {
            BIGNUM_MPI_CHK( Bignum_shift_r( &TU, 1 ) );

            if( ( U1.p[0] & 1 ) != 0 || ( U2.p[0] & 1 ) != 0 )
            {
                BIGNUM_MPI_CHK( Bignum_add_mpi( &U1, &U1, &TB ) );
                BIGNUM_MPI_CHK( Bignum_sub_mpi( &U2, &U2, &TA ) );
            }

            BIGNUM_MPI_CHK( Bignum_shift_r( &U1, 1 ) );
            BIGNUM_MPI_CHK( Bignum_shift_r( &U2, 1 ) );
        }

        while( ( TV.p[0] & 1 ) == 0 )
        {
            BIGNUM_MPI_CHK( Bignum_shift_r( &TV, 1 ) );

            if( ( V1.p[0] & 1 ) != 0 || ( V2.p[0] & 1 ) != 0 )
            {
                BIGNUM_MPI_CHK( Bignum_add_mpi( &V1, &V1, &TB ) );
                BIGNUM_MPI_CHK( Bignum_sub_mpi( &V2, &V2, &TA ) );
            }

            BIGNUM_MPI_CHK( Bignum_shift_r( &V1, 1 ) );
            BIGNUM_MPI_CHK( Bignum_shift_r( &V2, 1 ) );
        }

        if( Bignum_cmp_mpi( &TU, &TV ) >= 0 )
        {
            BIGNUM_MPI_CHK( Bignum_sub_mpi( &TU, &TU, &TV ) );
            BIGNUM_MPI_CHK( Bignum_sub_mpi( &U1, &U1, &V1 ) );
            BIGNUM_MPI_CHK( Bignum_sub_mpi( &U2, &U2, &V2 ) );
        }
        else
        {
            BIGNUM_MPI_CHK( Bignum_sub_mpi( &TV, &TV, &TU ) );
            BIGNUM_MPI_CHK( Bignum_sub_mpi( &V1, &V1, &U1 ) );
            BIGNUM_MPI_CHK( Bignum_sub_mpi( &V2, &V2, &U2 ) );
        }
    }
    while( Bignum_cmp_int( &TU, 0 ) != 0 );

    while( Bignum_cmp_int( &V1, 0 ) < 0 )
        BIGNUM_MPI_CHK( Bignum_add_mpi( &V1, &V1, N ) );

    while( Bignum_cmp_mpi( &V1, N ) >= 0 )
        BIGNUM_MPI_CHK( Bignum_sub_mpi( &V1, &V1, N ) );

    BIGNUM_MPI_CHK( Bignum_copy( X, &V1 ) );

cleanup:

    Bignum_free( &TA ); Bignum_free( &TU ); Bignum_free( &U1 ); Bignum_free( &U2 );
    Bignum_free( &G ); Bignum_free( &TB ); Bignum_free( &TV );
    Bignum_free( &V1 ); Bignum_free( &V2 );

    return( ret );
}

static const int small_prime[] =
{
        3,    5,    7,   11,   13,   17,   19,   23,
       29,   31,   37,   41,   43,   47,   53,   59,
       61,   67,   71,   73,   79,   83,   89,   97,
      101,  103,  107,  109,  113,  127,  131,  137,
      139,  149,  151,  157,  163,  167,  173,  179,
      181,  191,  193,  197,  199,  211,  223,  227,
      229,  233,  239,  241,  251,  257,  263,  269,
      271,  277,  281,  283,  293,  307,  311,  313,
      317,  331,  337,  347,  349,  353,  359,  367,
      373,  379,  383,  389,  397,  401,  409,  419,
      421,  431,  433,  439,  443,  449,  457,  461,
      463,  467,  479,  487,  491,  499,  503,  509,
      521,  523,  541,  547,  557,  563,  569,  571,
      577,  587,  593,  599,  601,  607,  613,  617,
      619,  631,  641,  643,  647,  653,  659,  661,
      673,  677,  683,  691,  701,  709,  719,  727,
      733,  739,  743,  751,  757,  761,  769,  773,
      787,  797,  809,  811,  821,  823,  827,  829,
      839,  853,  857,  859,  863,  877,  881,  883,
      887,  907,  911,  919,  929,  937,  941,  947,
      953,  967,  971,  977,  983,  991,  997, -103
};

/*
 * Small divisors test (X must be positive)
 *
 * Return values:
 * 0: no small factor (possible prime, more tests needed)
 * 1: certain prime
 * BIGNUM_ERR_MPI_NOT_ACCEPTABLE: certain non-prime
 * other negative: error
 */
static int mpi_check_small_factors( const Bignum *X )
{
    int ret = 0;
    size_t i;
    uintt r;

    if( ( X->p[0] & 1 ) == 0 )
        return( BIGNUM_ERR_MPI_NOT_ACCEPTABLE );

    for( i = 0; small_prime[i] > 0; i++ )
    {
        if( Bignum_cmp_int( X, small_prime[i] ) <= 0 )
            return( 1 );

        BIGNUM_MPI_CHK( Bignum_mod_int( &r, X, small_prime[i] ) );

        if( r == 0 )
            return( BIGNUM_ERR_MPI_NOT_ACCEPTABLE );
    }

cleanup:
    return( ret );
}

/*
 * Miller-Rabin pseudo-primality test  (HAC 4.24)
 */
static int mpi_miller_rabin( const Bignum *X, size_t rounds,
                             int (*f_rng)(void *, unsigned char *, size_t),
                             void *p_rng )
{
    int ret, count;
    size_t i, j, k, s;
    Bignum W, R, T, A, RR;

    MPI_VALIDATE_RET( X     != NULL );
    MPI_VALIDATE_RET( f_rng != NULL );

    Bignum_init( &W ); Bignum_init( &R );
    Bignum_init( &T ); Bignum_init( &A );
    Bignum_init( &RR );

    /*
     * W = |X| - 1
     * R = W >> lsb( W )
     */
    BIGNUM_MPI_CHK( Bignum_sub_int( &W, X, 1 ) );
    s = Bignum_lsb( &W );
    BIGNUM_MPI_CHK( Bignum_copy( &R, &W ) );
    BIGNUM_MPI_CHK( Bignum_shift_r( &R, s ) );

    for( i = 0; i < rounds; i++ )
    {
        /*
         * pick a random A, 1 < A < |X| - 1
         */
        count = 0;
        do {
            BIGNUM_MPI_CHK( Bignum_fill_random( &A, X->n * ciL, f_rng, p_rng ) );

            j = Bignum_bitlen( &A );
            k = Bignum_bitlen( &W );
            if (j > k) {
                A.p[A.n - 1] &= ( (uintt) 1 << ( k - ( A.n - 1 ) * biL - 1 ) ) - 1;
            }

            if (count++ > 30) {
                ret = BIGNUM_ERR_MPI_NOT_ACCEPTABLE;
                goto cleanup;
            }

        } while ( Bignum_cmp_mpi( &A, &W ) >= 0 ||
                  Bignum_cmp_int( &A, 1 )  <= 0    );

        /*
         * A = A^R mod |X|
         */
        BIGNUM_MPI_CHK( Bignum_exp_mod( &A, &A, &R, X, &RR ) );

        if( Bignum_cmp_mpi( &A, &W ) == 0 ||
            Bignum_cmp_int( &A,  1 ) == 0 )
            continue;

        j = 1;
        while( j < s && Bignum_cmp_mpi( &A, &W ) != 0 )
        {
            /*
             * A = A * A mod |X|
             */
            BIGNUM_MPI_CHK( Bignum_mul_mpi( &T, &A, &A ) );
            BIGNUM_MPI_CHK( Bignum_mod_mpi( &A, &T, X  ) );

            if( Bignum_cmp_int( &A, 1 ) == 0 )
                break;

            j++;
        }

        /*
         * not prime if A != |X| - 1 or A == 1
         */
        if( Bignum_cmp_mpi( &A, &W ) != 0 ||
            Bignum_cmp_int( &A,  1 ) == 0 )
        {
            ret = BIGNUM_ERR_MPI_NOT_ACCEPTABLE;
            break;
        }
    }

cleanup:
    Bignum_free( &W ); Bignum_free( &R );
    Bignum_free( &T ); Bignum_free( &A );
    Bignum_free( &RR );

    return( ret );
}

/*
 * Pseudo-primality test: small factors, then Miller-Rabin
 */
int Bignum_is_prime_ext( const Bignum *X, int rounds,
                              int (*f_rng)(void *, unsigned char *, size_t),
                              void *p_rng )
{
    int ret = BIGNUM_ERR_ERROR_CORRUPTION_DETECTED;
    Bignum XX;
    MPI_VALIDATE_RET( X     != NULL );
    MPI_VALIDATE_RET( f_rng != NULL );

    XX.s = 1;
    XX.n = X->n;
    XX.p = X->p;

    if( Bignum_cmp_int( &XX, 0 ) == 0 ||
        Bignum_cmp_int( &XX, 1 ) == 0 )
        return( BIGNUM_ERR_MPI_NOT_ACCEPTABLE );

    if( Bignum_cmp_int( &XX, 2 ) == 0 )
        return( 0 );

    if( ( ret = mpi_check_small_factors( &XX ) ) != 0 )
    {
        if( ret == 1 )
            return( 0 );

        return( ret );
    }

    return( mpi_miller_rabin( &XX, rounds, f_rng, p_rng ) );
}

/*
 * Prime number generation
 *
 * To generate an RSA key in a way recommended by FIPS 186-4, both primes must
 * be either 1024 bits or 1536 bits long, and flags must contain
 * BIGNUM_MPI_GEN_PRIME_FLAG_LOW_ERR.
 */
int Bignum_gen_prime( Bignum *X, size_t nbits, int flags,
                   int (*f_rng)(void *, unsigned char *, size_t),
                   void *p_rng )
{
#ifdef BIGNUM_HAVE_INT64
// ceil(2^63.5)
#define CEIL_MAXUINT_DIV_SQRT2 0xb504f333f9de6485ULL
#else
// ceil(2^31.5)
#define CEIL_MAXUINT_DIV_SQRT2 0xb504f334U
#endif
    int ret = BIGNUM_ERR_MPI_NOT_ACCEPTABLE;
    size_t k, n;
    int rounds;
    uintt r;
    Bignum Y;

    MPI_VALIDATE_RET( X     != NULL );
    MPI_VALIDATE_RET( f_rng != NULL );

    if( nbits < 3 || nbits > BIGNUM_MPI_MAX_BITS )
        return( BIGNUM_ERR_MPI_BAD_INPUT_DATA );

    Bignum_init( &Y );

    n = BITS_TO_LIMBS( nbits );

    if( ( flags & BIGNUM_MPI_GEN_PRIME_FLAG_LOW_ERR ) == 0 )
    {
        /*
         * 2^-80 error probability, number of rounds chosen per HAC, table 4.4
         */
        rounds = ( ( nbits >= 1300 ) ?  2 : ( nbits >=  850 ) ?  3 :
                   ( nbits >=  650 ) ?  4 : ( nbits >=  350 ) ?  8 :
                   ( nbits >=  250 ) ? 12 : ( nbits >=  150 ) ? 18 : 27 );
    }
    else
    {
        /*
         * 2^-100 error probability, number of rounds computed based on HAC,
         * fact 4.48
         */
        rounds = ( ( nbits >= 1450 ) ?  4 : ( nbits >=  1150 ) ?  5 :
                   ( nbits >= 1000 ) ?  6 : ( nbits >=   850 ) ?  7 :
                   ( nbits >=  750 ) ?  8 : ( nbits >=   500 ) ? 13 :
                   ( nbits >=  250 ) ? 28 : ( nbits >=   150 ) ? 40 : 51 );
    }

    while( 1 )
    {
        BIGNUM_MPI_CHK( Bignum_fill_random( X, n * ciL, f_rng, p_rng ) );
        /* make sure generated number is at least (nbits-1)+0.5 bits (FIPS 186-4 §B.3.3 steps 4.4, 5.5) */
        if( X->p[n-1] < CEIL_MAXUINT_DIV_SQRT2 ) continue;

        k = n * biL;
        if( k > nbits ) BIGNUM_MPI_CHK( Bignum_shift_r( X, k - nbits ) );
        X->p[0] |= 1;

        if( ( flags & BIGNUM_MPI_GEN_PRIME_FLAG_DH ) == 0 )
        {
            ret = Bignum_is_prime_ext( X, rounds, f_rng, p_rng );

            if( ret != BIGNUM_ERR_MPI_NOT_ACCEPTABLE )
                goto cleanup;
        }
        else
        {
            /*
             * An necessary condition for Y and X = 2Y + 1 to be prime
             * is X = 2 mod 3 (which is equivalent to Y = 2 mod 3).
             * Make sure it is satisfied, while keeping X = 3 mod 4
             */

            X->p[0] |= 2;

            BIGNUM_MPI_CHK( Bignum_mod_int( &r, X, 3 ) );
            if( r == 0 )
                BIGNUM_MPI_CHK( Bignum_add_int( X, X, 8 ) );
            else if( r == 1 )
                BIGNUM_MPI_CHK( Bignum_add_int( X, X, 4 ) );

            /* Set Y = (X-1) / 2, which is X / 2 because X is odd */
            BIGNUM_MPI_CHK( Bignum_copy( &Y, X ) );
            BIGNUM_MPI_CHK( Bignum_shift_r( &Y, 1 ) );

            while( 1 )
            {
                /*
                 * First, check small factors for X and Y
                 * before doing Miller-Rabin on any of them
                 */
                if( ( ret = mpi_check_small_factors(  X         ) ) == 0 &&
                    ( ret = mpi_check_small_factors( &Y         ) ) == 0 &&
                    ( ret = mpi_miller_rabin(  X, rounds, f_rng, p_rng  ) )
                                                                    == 0 &&
                    ( ret = mpi_miller_rabin( &Y, rounds, f_rng, p_rng  ) )
                                                                    == 0 )
                    goto cleanup;

                if( ret != BIGNUM_ERR_MPI_NOT_ACCEPTABLE )
                    goto cleanup;

                /*
                 * Next candidates. We want to preserve Y = (X-1) / 2 and
                 * Y = 1 mod 2 and Y = 2 mod 3 (eq X = 3 mod 4 and X = 2 mod 3)
                 * so up Y by 6 and X by 12.
                 */
                BIGNUM_MPI_CHK( Bignum_add_int(  X,  X, 12 ) );
                BIGNUM_MPI_CHK( Bignum_add_int( &Y, &Y, 6  ) );
            }
        }
    }

cleanup:

    Bignum_free( &Y );

    return( ret );
}

void Bignum_Init(Bignum *bn){
    Bignum_init(bn);
}

void Bignum_Free(Bignum *bn){
    Bignum_free(bn);
}

void Bignum_FromString(Bignum *bn, const char *str, int radix){
    Bignum_init(bn);
    int rc = Bignum_read_string(bn, radix, str);
    if(rc != 0){
        printf("Failed to read string\n");
    }
}

void Bignum_ToString(Bignum *bn, char *str, size_t buflen, size_t *olen, int radix){
    (void)Bignum_write_string(bn, radix, str, buflen, olen);
}

void Bignum_Add(Bignum *C, Bignum *A, Bignum *B){
    Bignum_init(C);
    int rc = Bignum_add_mpi(C, A, B);
    if(rc != 0){
        printf("Failed to add bignums\n");
    }
}

void Bignum_Sub(Bignum *C, Bignum *A, Bignum *B){
    Bignum_init(C);
    int rc = Bignum_sub_mpi(C, A, B);
    if(rc != 0){
        printf("Failed to sub bignums\n");
    }
}
