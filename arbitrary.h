#ifndef ARBITRARY_H
#define ARBITRARY_H
#include <stddef.h>     // size_t
#include <stdint.h>     // uint8_t
#ifndef __cplusplus
#include <stdbool.h>    // bool
#endif

// Ensures attribute portability
#ifdef __GNUC__
#define attribute(...)  __attribute__((__VA_ARGS__))
#define FPCONV_INIT 0   // 'fpconv_init()' need not be called at start
#else
#define attribute(...)
#define FPCONV_INIT 1   // 'fpconv_init()' MUST be called at start
#endif  // glibc || libstdc++
#ifdef _MSC_VER
#define declspec(...)   __declspec(__VA_ARGS__)
#else
#define declspec(...)
#endif  // MSVC

// Use implementation-specific 'restrict' for C++, if possible
#ifdef __cplusplus
#ifdef __GNUC__
#define restrict    __restrict__
#elif defined(_MSC_VER)
#define restrict    __restrict
#else
#define restrict
#endif
#endif  // C++

#define dynint_neg(value)       dynint_negeq(dynint_copy(value))
#define dynint_negeq(target)    dynint_addeq(dynint_noteq(target), dynint_one)

// Floating-point rithmetic options
typedef enum arithflag_t {
    AF_NULL,        // No flags
    AF_ROUND = 64,  // Round to # of decimal places
    AF_FLOOR = 128, // Floor result
    AF_CEIL  = 256  // Ceiling result
} arithflag_t;

/* Printing options
 * By default, ... */
typedef enum printflag_t {
    PF_NULL,        // No flags
    PF_SIGF = 64,   // Determines # of significant figures
    PF_FULL = 128,  // Print to full precision
    PF_SCIN = 256   // Always print in scientific notation
} printflag_t;

// Integral type used to store data within arbitrary-precision numbers
typedef uintmax_t bitfield_t;

// Arbitrary-precision integer
typedef struct dynint_t {
    bitfield_t *bits;
    size_t size;
} dynint_t;

// Arbitrary-precision floating-point number
typedef struct dynfloat_t {
    bitfield_t *whole, *decimal;
    size_t wsize, dsize;
} dynfloat_t;

const dynint_t *dynint_one = &(dynint_t) {(bitfield_t[]) {1}, 1};

// ---- dynfloat_t Functions ----

void dynfloat_free(dynfloat_t *value);
dynfloat_t *dynfloat_copy(const dynfloat_t *restrict value);
dynfloat_t *dynfloat_new(void);

// ---- dynint_t Functions ----

dynint_t *dynint_add(const dynint_t *value1, const dynint_t *value2);
dynint_t *dynint_addeq(dynint_t *target, const dynint_t *value);
dynint_t *dynint_abs(const dynint_t *restrict value);
dynint_t *dynint_abseq(dynint_t *value);
dynint_t *dynint_and(const dynint_t *value1, const dynint_t *value2);
dynint_t *dynint_andeq(dynint_t *target, const dynint_t *value);
int dynint_cmp(const dynint_t *value1, const dynint_t *value2);
dynint_t *dynint_copy(const dynint_t *restrict value);
void dynint_free(dynint_t *value);
dynint_t *dynint_new(void);
dynint_t *dynint_not(const dynint_t *value);
dynint_t *dynint_noteq(dynint_t *target);
dynint_t *dynint_or(const dynint_t *value1, const dynint_t *value2);
dynint_t *dynint_oreq(dynint_t *target, const dynint_t *value);
dynint_t *dynint_sub(const dynint_t *value1, const dynint_t *value2);
dynint_t *dynint_subeq(dynint_t *target, const dynint_t *value);
dynint_t *dynint_xor(const dynint_t *value1, const dynint_t *value2);
dynint_t *dynint_xoreq(dynint_t *target, const dynint_t *value);

#endif
