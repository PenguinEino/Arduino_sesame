/* Copyright 2014, Kenneth MacKay. Licensed under the BSD 2-clause license. */

#ifndef _UECC_H_
#define _UECC_H_

#include <stdint.h>

/* Platform selection options.
If uECC_PLATFORM is not defined, the code will try to guess it based on compiler macros.
Possible values for uECC_PLATFORM are defined below: */
#define uECC_arch_other 0
#define uECC_x86 1
#define uECC_x86_64 2
#define uECC_arm 3
#define uECC_arm_thumb 4
#define uECC_arm_thumb2 5
#define uECC_arm64 6
#define uECC_avr 7

/* If desired, you can define uECC_WORD_SIZE as appropriate for your platform (1, 4, or 8 bytes).
If uECC_WORD_SIZE is not explicitly defined then it will be automatically set based on your
platform. */

/* Optimization level; trade speed for code size.
   Larger values produce code that is faster but larger.
   Currently supported values are 0 - 4; 0 is unusably slow for most applications.
   Optimization level 4 currently only has an effect ARM platforms where more than one
   curve is enabled. */
#ifndef uECC_OPTIMIZATION_LEVEL
#define uECC_OPTIMIZATION_LEVEL 2
#endif

/* uECC_SQUARE_FUNC - If enabled (defined as nonzero), this will cause a specific function to be
used for (scalar) squaring instead of the generic multiplication function. This can make things
faster somewhat faster, but increases the code size. */
#ifndef uECC_SQUARE_FUNC
#define uECC_SQUARE_FUNC 0
#endif

/* uECC_VLI_NATIVE_LITTLE_ENDIAN - If enabled (defined as nonzero), this will switch to native
little-endian format for *all* arrays passed in and out of the public API. This includes public
and private keys, shared secrets, signatures and message hashes.
Using this switch reduces the amount of call stack memory used by uECC, since less intermediate
translations are required.
Note that this will *only* work on native little-endian processors and it will treat the uint8_t
arrays passed into the public API as word arrays, therefore requiring the provided byte arrays
to be word aligned on architectures that do not support unaligned accesses.
IMPORTANT: Keys and signatures generated with uECC_VLI_NATIVE_LITTLE_ENDIAN=1 are incompatible
with keys and signatures generated with uECC_VLI_NATIVE_LITTLE_ENDIAN=0; all parties must use
the same endianness. */
#ifndef uECC_VLI_NATIVE_LITTLE_ENDIAN
#define uECC_VLI_NATIVE_LITTLE_ENDIAN 1
#endif

/* Curve support selection. Set to 0 to remove that curve. */
#ifndef uECC_SUPPORTS_secp160r1
#define uECC_SUPPORTS_secp160r1 0
#endif
#ifndef uECC_SUPPORTS_secp192r1
#define uECC_SUPPORTS_secp192r1 1
#endif
#ifndef uECC_SUPPORTS_secp224r1
#define uECC_SUPPORTS_secp224r1 0
#endif
#ifndef uECC_SUPPORTS_secp256r1
#define uECC_SUPPORTS_secp256r1 1
#endif
#ifndef uECC_SUPPORTS_secp256k1
#define uECC_SUPPORTS_secp256k1 0
#endif

/* Specifies whether compressed point format is supported.
   Set to 0 to disable point compression/decompression functions. */
#ifndef uECC_SUPPORT_COMPRESSED_POINT
#define uECC_SUPPORT_COMPRESSED_POINT 0
#endif

struct uECC_Curve_t;
typedef const struct uECC_Curve_t * uECC_Curve;

#ifdef __cplusplus
extern "C" {
#endif

#if uECC_SUPPORTS_secp160r1
uECC_Curve uECC_secp160r1(void);
#endif
#if uECC_SUPPORTS_secp192r1
uECC_Curve uECC_secp192r1(void);
#endif
#if uECC_SUPPORTS_secp224r1
uECC_Curve uECC_secp224r1(void);
#endif
#if uECC_SUPPORTS_secp256r1
uECC_Curve uECC_secp256r1(void);
#endif
#if uECC_SUPPORTS_secp256k1
uECC_Curve uECC_secp256k1(void);
#endif

/* uECC_RNG_Function type
The RNG function should fill 'size' random bytes into 'dest'. It should return 1 if
'dest' was filled with random data, or 0 if the random data could not be generated.
The filled-in values should be either truly random, or from a cryptographically-secure PRNG.

A correctly functioning RNG function must be set (using uECC_set_rng()) before calling
uECC_make_key() or uECC_sign().

Setting a correctly functioning RNG function improves the resistance to side-channel attacks
for uECC_shared_secret() and uECC_sign_deterministic().

A correct RNG function is set by default when building for Windows, Linux, or OS X.
If you are building on another POSIX-compliant system that supports /dev/random or /dev/urandom,
you can define uECC_POSIX to use the predefined RNG. For embedded platforms there is no predefined
RNG function; you must provide your own.
*/
typedef int (*uECC_RNG_Function)(uint8_t * dest, unsigned size);

int uECC_make_key_lit(uint8_t * public_key, uint8_t * private_key, uECC_Curve curve);

int uECC_shared_secret_lit(const uint8_t * public_key, const uint8_t * private_key, uint8_t * secret, uECC_Curve curve);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* _UECC_H_ */
