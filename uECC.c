/* Copyright 2014, Kenneth MacKay. Licensed under the BSD 2-clause license. */

#include "uECC.h"

#ifndef uECC_RNG_MAX_TRIES
#define uECC_RNG_MAX_TRIES 64
#endif

#if uECC_ENABLE_VLI_API
#define uECC_VLI_API
#else
#define uECC_VLI_API static
#endif

#define CONCATX(a, ...) a##__VA_ARGS__
#define CONCAT(a, ...) CONCATX(a, __VA_ARGS__)

#define STRX(a) #a
#define STR(a) STRX(a)

#define EVAL(...) EVAL1(EVAL1(EVAL1(EVAL1(__VA_ARGS__))))
#define EVAL1(...) EVAL2(EVAL2(EVAL2(EVAL2(__VA_ARGS__))))
#define EVAL2(...) EVAL3(EVAL3(EVAL3(EVAL3(__VA_ARGS__))))
#define EVAL3(...) EVAL4(EVAL4(EVAL4(EVAL4(__VA_ARGS__))))
#define EVAL4(...) __VA_ARGS__

#define DEC_1 0
#define DEC_2 1
#define DEC_3 2
#define DEC_4 3
#define DEC_5 4
#define DEC_6 5
#define DEC_7 6
#define DEC_8 7
#define DEC_9 8
#define DEC_10 9
#define DEC_11 10
#define DEC_12 11
#define DEC_13 12
#define DEC_14 13
#define DEC_15 14
#define DEC_16 15
#define DEC_17 16
#define DEC_18 17
#define DEC_19 18
#define DEC_20 19
#define DEC_21 20
#define DEC_22 21
#define DEC_23 22
#define DEC_24 23
#define DEC_25 24
#define DEC_26 25
#define DEC_27 26
#define DEC_28 27
#define DEC_29 28
#define DEC_30 29
#define DEC_31 30
#define DEC_32 31

#define DEC(N) CONCAT(DEC_, N)

#define SECOND_ARG(_, val, ...) val
#define SOME_CHECK_0 ~, 0
#define GET_SECOND_ARG(...) SECOND_ARG(__VA_ARGS__, SOME, )
#define SOME_OR_0(N) GET_SECOND_ARG(CONCAT(SOME_CHECK_, N))

#define EMPTY(...)
#define DEFER(...) __VA_ARGS__ EMPTY()

#define REPEAT_NAME_0() REPEAT_0
#define REPEAT_NAME_SOME() REPEAT_SOME
#define REPEAT_0(...)
#define REPEAT_SOME(N, stuff) DEFER(CONCAT(REPEAT_NAME_, SOME_OR_0(DEC(N))))()(DEC(N), stuff) stuff
#define REPEAT(N, stuff) EVAL(REPEAT_SOME(N, stuff))

#define REPEATM_NAME_0() REPEATM_0
#define REPEATM_NAME_SOME() REPEATM_SOME
#define REPEATM_0(...)
#define REPEATM_SOME(N, macro) macro(N) DEFER(CONCAT(REPEATM_NAME_, SOME_OR_0(DEC(N))))()(DEC(N), macro)
#define REPEATM(N, macro) EVAL(REPEATM_SOME(N, macro))

#include "types.h"

#if (uECC_WORD_SIZE == 1)
#if uECC_SUPPORTS_secp160r1
#define uECC_MAX_WORDS 21 /* Due to the size of curve_n. */
#endif
#if uECC_SUPPORTS_secp192r1
#undef uECC_MAX_WORDS
#define uECC_MAX_WORDS 24
#endif
#if uECC_SUPPORTS_secp224r1
#undef uECC_MAX_WORDS
#define uECC_MAX_WORDS 28
#endif
#if (uECC_SUPPORTS_secp256r1 || uECC_SUPPORTS_secp256k1)
#undef uECC_MAX_WORDS
#define uECC_MAX_WORDS 32
#endif
#elif (uECC_WORD_SIZE == 4)
#if uECC_SUPPORTS_secp160r1
#define uECC_MAX_WORDS 6 /* Due to the size of curve_n. */
#endif
#if uECC_SUPPORTS_secp192r1
#undef uECC_MAX_WORDS
#define uECC_MAX_WORDS 6
#endif
#if uECC_SUPPORTS_secp224r1
#undef uECC_MAX_WORDS
#define uECC_MAX_WORDS 7
#endif
#if (uECC_SUPPORTS_secp256r1 || uECC_SUPPORTS_secp256k1)
#undef uECC_MAX_WORDS
#define uECC_MAX_WORDS 8
#endif
#elif (uECC_WORD_SIZE == 8)
#if uECC_SUPPORTS_secp160r1
#define uECC_MAX_WORDS 3
#endif
#if uECC_SUPPORTS_secp192r1
#undef uECC_MAX_WORDS
#define uECC_MAX_WORDS 3
#endif
#if uECC_SUPPORTS_secp224r1
#undef uECC_MAX_WORDS
#define uECC_MAX_WORDS 4
#endif
#if (uECC_SUPPORTS_secp256r1 || uECC_SUPPORTS_secp256k1)
#undef uECC_MAX_WORDS
#define uECC_MAX_WORDS 4
#endif
#endif /* uECC_WORD_SIZE */

#define BITS_TO_WORDS(num_bits) ((num_bits + ((uECC_WORD_SIZE * 8) - 1)) / (uECC_WORD_SIZE * 8))
#define BITS_TO_BYTES(num_bits) ((num_bits + 7) / 8)

struct uECC_Curve_t
{
    wordcount_t num_words;
    wordcount_t num_bytes;
    bitcount_t num_n_bits;
    uECC_word_t p[uECC_MAX_WORDS];
    uECC_word_t n[uECC_MAX_WORDS];
    uECC_word_t G[uECC_MAX_WORDS * 2];
    uECC_word_t b[uECC_MAX_WORDS];

    void (*double_jacobian)(uECC_word_t * X1, uECC_word_t * Y1, uECC_word_t * Z1, uECC_Curve curve);

#if uECC_SUPPORT_COMPRESSED_POINT
    void (*mod_sqrt)(uECC_word_t * a, uECC_Curve curve);
#endif

    void (*x_side)(uECC_word_t * result, const uECC_word_t * x, uECC_Curve curve);

#if (uECC_OPTIMIZATION_LEVEL > 0)

    void (*mmod_fast)(uECC_word_t * result, uECC_word_t * product);

#endif
};

#if uECC_VLI_NATIVE_LITTLE_ENDIAN

static void __bocpy(uint8_t * dst, const uint8_t * src, unsigned num_bytes)
{
    while (0 != num_bytes)
    {
        num_bytes--;
        dst[num_bytes] = src[num_bytes];
    }
}

#endif

static cmpresult_t uECC_vli_cmp_unsafe(const uECC_word_t * left, const uECC_word_t * right, wordcount_t num_words);

#if (uECC_PLATFORM == uECC_arm || uECC_PLATFORM == uECC_arm_thumb || uECC_PLATFORM == uECC_arm_thumb2)
#include "asm_arm.inc"
#endif

#if (uECC_PLATFORM == uECC_avr)
#include "asm_avr.inc"
#endif

#if default_RNG_defined
static uECC_RNG_Function g_rng_function = &default_RNG;
#else
static uECC_RNG_Function g_rng_function = 0;
#endif


int uECC_make_key_lit(uint8_t * public_key, uint8_t * private_key, uECC_Curve curve)
{

    uECC_word_t _private[uECC_MAX_WORDS];
    uECC_word_t _public[uECC_MAX_WORDS * 2];
    uECC_word_t tries;

    for (tries = 0; tries < uECC_RNG_MAX_TRIES; ++tries)
    {
        if (!uECC_generate_random_int(_private, curve->n, BITS_TO_WORDS(curve->num_n_bits)))
        {
            return 0;
        }

        if (EccPoint_compute_public_key(_public, _private, curve))
        {
            uECC_vli_nativeToBytes(private_key, BITS_TO_BYTES(curve->num_n_bits), _private);
            uECC_vli_nativeToBytes(public_key, curve->num_bytes, _public);
            uECC_vli_nativeToBytes(public_key + curve->num_bytes, curve->num_bytes, _public + curve->num_words);
            return 1;
        }
    }
    return 0;
}

int uECC_shared_secret_lit(const uint8_t * public_key, const uint8_t * private_key, uint8_t * secret, uECC_Curve curve)
{
    uECC_word_t _public[uECC_MAX_WORDS * 2];
    uECC_word_t _private[uECC_MAX_WORDS];

    uECC_word_t tmp[uECC_MAX_WORDS];
    uECC_word_t * p2[2]     = { _private, tmp };
    uECC_word_t * initial_Z = 0;
    uECC_word_t carry;
    wordcount_t num_words = curve->num_words;
    wordcount_t num_bytes = curve->num_bytes;

    uECC_vli_bytesToNative(_private, private_key, BITS_TO_BYTES(curve->num_n_bits));
    uECC_vli_bytesToNative(_public, public_key, num_bytes);
    uECC_vli_bytesToNative(_public + num_words, public_key + num_bytes, num_bytes);

    /* Regularize the bitcount for the private key so that attackers cannot use a side channel
       attack to learn the number of leading zeros. */
    carry = regularize_k(_private, _private, tmp, curve);

    /* If an RNG function was specified, try to get a random initial Z value to improve
       protection against side-channel attacks. */
    if (g_rng_function)
    {
        if (!uECC_generate_random_int(p2[carry], curve->p, num_words))
        {
            return 0;
        }
        initial_Z = p2[carry];
    }

    EccPoint_mult(_public, _public, p2[!carry], initial_Z, curve->num_n_bits + 1, curve);

    uECC_vli_nativeToBytes(secret, num_bytes, _public);
    return !EccPoint_isZero(_public, curve);
}
