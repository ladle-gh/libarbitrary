#ifndef LADLE_ARBITRARY_H
#define LADLE_ARBITRARY_H
#include <stddef.h> // size_t
#include <stdint.h> // intmax_t, SIZE_MAX, uintmax_t, UINT#_MAX
#include <stdio.h>  // printf()
#include <stdlib.h> // malloc()

#include <ladle/common/header.h>
#include <ladle/common/limits.h>    // uint_most64_t

// ---- Constants ----

// Integral type used to store data within arbitrary-precision numbers
typedef uint_most64_t bitfld_t;

// Arbitrary-precision decimal
typedef struct {
    bitfld_t *whl, *dec;
    size_t wsize, dsize;
    enum {
        AF_NULL,        // No flags
        AF_ROUND,       // Round to # of decimal places
        AF_FLOOR = 128, // Floor result
        AF_CEIL  = 256  // Ceiling result
    } rules;
    bool rval;
} ddec_t;

// Arbitrary-precision integer
typedef struct {
    bitfld_t *bits;
    size_t size;
    bool rval;
} dwhl_t;

// Printing options
typedef enum {
    PF_NULL,        // No flags
    PF_SIGF,        // Determines # of significant figures
    PF_FULL = 128,  // Print to full precision
    PF_SCIN = 256   // Always print in scientific notation
} dprint_t;

// Generic fundamental types
typedef long double floatp_t;
typedef intmax_t integr_t;
typedef uintmax_t uintegr_t;

// Integral type used to shift bits within integers
#if defined(UINT64_MAX)   && (UINT64_MAX / (UINT_MOST64_SIZE * 8) <= SIZE_MAX)
#define SHIFT_MAX   UINT64_MAX
typedef uint64_t shift_t;
#elif defined(UINT32_MAX) && (UINT32_MAX / (UINT_MOST64_SIZE * 8) <= SIZE_MAX)
#define SHIFT_MAX   UINT32_MAX
typedef uint32_t shift_t;
#elif defined(UINT16_MAX) && (UINT16_MAX / (UINT_MOST64_SIZE * 8) <= SIZE_MAX)
#define SHIFT_MAX   UINT16_MAX
typedef uint16_t shift_t;
#elif defined(UINT8_MAX)  && (UINT8_MAX  / (UINT_MOST64_SIZE * 8) <= SIZE_MAX)
#define SHIFT_MAX   UINT8_MAX
typedef uint8_t shift_t;
#endif

// Type returned by `sh_div()'
typedef struct {
    shift_t quot, rem;
} shdiv_t;

// ---- ddec_t ----

// ...

// ---- dwhl_t ----

import extern const dwhl_t *const dwhl_one;
import extern const dwhl_t *const dwhl_zero;

BEGIN

// -- Basic Utilities --

/* Returns integral cast of integer of given size, in bytes
 * Returns 0 and sets errno in internal error */
import integr_t dwhl_casts(const dwhl_t *val, size_t size) nonnull() pure;
import uintegr_t dwhl_castu(const dwhl_t *val, size_t size) nonnull() pure;

/* Compares two integers
 * Returns NULL and sets errno on internal error
 *
 * Case            Return
 *  lhs < rhs     -1
 *  lhs > rhs      1
 *  lhs = rhs      0 */
import int dwhl_cmp(const dwhl_t *lhs, const dwhl_t *rhs) nonnull() pure;

/* Assigns value to tar
 * Returns NULL and sets errno on internal error */
import dwhl_t *dwhl_eq(dwhl_t *restrict tar, const dwhl_t *restrict val) nonnull();
// import dwhl_t *dwhl_eqf(dwhl_t *restrict tar, const ddec_t *restrict val) nonnull();
import dwhl_t *dwhl_eqs(dwhl_t *restrict tar, integr_t val) nonnull();
import dwhl_t *dwhl_equ(dwhl_t *restrict tar, uintegr_t val) nonnull();

/* Assigns initial value to tar
 * Returns NULL and sets errno on internal error */
// import dwhl_t *dwhl_initf(dwhl_t *restrict tar, const ddec_t *restrict val) nonnull();
import dwhl_t *dwhl_initi(dwhl_t *restrict tar, const dwhl_t *restrict val) nonnull();
import dwhl_t *dwhl_inits(dwhl_t *restrict tar, integr_t val) nonnull();
import dwhl_t *dwhl_initu(dwhl_t *restrict tar, uintegr_t val) nonnull();

// Returns true if integer is negative
import bool dwhl_isneg(const dwhl_t *restrict val) nonnull();

/* Swaps values of integers
 * Returns pointer to first integer
 * Returns NULL and sets errno if NULL is passed */
import dwhl_t *dwhl_swp(dwhl_t *restrict ret, dwhl_t *restrict val) nonnull();

END

// Frees contents of integer
static inline void dwhl_clr(dwhl_t *val) nonnull();

// Creates temporary integer that can be passed as argument
static inline dwhl_t *dwhl_tmp(const dwhl_t *val) nonnull() warn_unused;
// static inline dwhl_t *dwhl_tmpf(const ddec_t *val) nonnull() warn_unused;
static inline dwhl_t *dwhl_tmps(integr_t val) warn_unused;
static inline dwhl_t *dwhl_tmpu(uintegr_t val) warn_unused;

void dwhl_clr(dwhl_t *restrict val) {
    if (val)
        free(val->bits);
    return;
}

dwhl_t *dwhl_tmp(const dwhl_t *val) {
    dwhl_t *tmp = dwhl_initi(malloc(sizeof(dwhl_t)), val);

    if (tmp)
        tmp->rval = true;
    return tmp;
}
/* dwhl_t *dwhl_tmpf(const ddec_t *val) {
    dwhl_t *tmp = dwhl_initf(malloc(sizeof(dwhl_t)), val);

    if (tmp)
        tmp->rval = true;
    return tmp;
} */
dwhl_t *dwhl_tmps(integr_t val) {
    dwhl_t *tmp = dwhl_inits(malloc(sizeof(dwhl_t)), val);

    if (tmp)
        tmp->rval = true;
    return tmp;
}
dwhl_t *dwhl_tmpu(uintegr_t val) {
    dwhl_t *tmp = dwhl_initu(malloc(sizeof(dwhl_t)), val);

    if (tmp)
        tmp->rval = true;
    return tmp;
}

BEGIN

// -- Basic Arithmetic --

/* Returns result of arithmetic/bitwise operation on two integers
 * Functions ending in '-eq' store result within `tar'
 * All other functions store result within heap-allocated copy
 * Returns NULL and sets errno on internal error */
import dwhl_t *dwhl_abseq(dwhl_t *tar) nonnull();
import dwhl_t *dwhl_addeq(dwhl_t *tar, const dwhl_t *val) nonnull();
import dwhl_t *dwhl_andeq(dwhl_t *tar, const dwhl_t *val) nonnull();
import dwhl_t *dwhl_negeq(dwhl_t *tar) nonnull();
import dwhl_t *dwhl_noteq(dwhl_t *tar) nonnull();
import dwhl_t *dwhl_oreq(dwhl_t *tar, const dwhl_t *val) nonnull();
import dwhl_t *dwhl_subeq(dwhl_t *tar, const dwhl_t *val) nonnull();
import dwhl_t *dwhl_xoreq(dwhl_t *tar, const dwhl_t *val) nonnull();

import dwhl_t *dwhl_abs(const dwhl_t *val) nonnull() warn_unused;
import dwhl_t *dwhl_neg(const dwhl_t *val) nonnull() warn_unused;
import dwhl_t *dwhl_add(const dwhl_t *lhs, const dwhl_t *rhs) nonnull() warn_unused;
import dwhl_t *dwhl_and(const dwhl_t *lhs, const dwhl_t *rhs) nonnull() warn_unused;
import dwhl_t *dwhl_or(const dwhl_t *lhs, const dwhl_t *rhs) nonnull() warn_unused;
import dwhl_t *dwhl_not(const dwhl_t *val) nonnull() warn_unused;
import dwhl_t *dwhl_sub(const dwhl_t *lhs, const dwhl_t *rhs) nonnull() warn_unused;
import dwhl_t *dwhl_xor(const dwhl_t *lhs, const dwhl_t *rhs) nonnull() warn_unused;

// Nonwhole results are truncated
import dwhl_t *dwhl_diveq(dwhl_t *tar, const dwhl_t *val) nonnull();
import dwhl_t *dwhl_modeq(dwhl_t *tar, const dwhl_t *val) nonnull();
import dwhl_t *dwhl_muleq(dwhl_t *tar, const dwhl_t *val) nonnull();

import dwhl_t *dwhl_div(const dwhl_t *lhs, const dwhl_t *rhs) nonnull() warn_unused;
import dwhl_t *dwhl_mod(const dwhl_t *lhs, const dwhl_t *rhs) nonnull() warn_unused;
import dwhl_t *dwhl_mul(const dwhl_t *lhs, const dwhl_t *rhs) nonnull() warn_unused;

/* Bits shifted right out-of-bounds will be saved
 * Bits shifted left out-of-bounds, however, will not be */
import dwhl_t *dwhl_lshifteq(dwhl_t *tar, shift_t shift) nonnull();
import dwhl_t *dwhl_slshifteq(dwhl_t *tar, shift_t shift) nonnull();
import dwhl_t *dwhl_rshifteq(dwhl_t *tar, shift_t shift) nonnull();

import dwhl_t *dwhl_lshift(const dwhl_t *val, shift_t shift) nonnull() warn_unused;
import dwhl_t *dwhl_slshift(const dwhl_t *val, shift_t shift) nonnull() warn_unused;
import dwhl_t *dwhl_rshift(const dwhl_t *val, shift_t shift) nonnull() warn_unused;

END

// ---- shift_t ----

static inline shdiv_t sh_div(shift_t num, shift_t denom) pure;

// Returns result of divions between two `shift_t's
shdiv_t sh_div(shift_t num, shift_t denom) {
    // TODO optimize in assembly
    return (shdiv_t) {num / denom, num % denom};
}

#include <ladle/common/end_header.h>
#endif  // #ifndef LADLE_ARBITRARY_H
