#ifndef LADLE_ARBITRARY_H
#define LADLE_ARBITRARY_H
#include <stddef.h> // size_t
#include <stdint.h> // uintmax_t
#include <stdlib.h> // free(), malloc()

#include <ladle/collect.h>
#include <ladle/common/header.h>

// ---- Constants ----

// Floating-point arithmetic options
typedef enum arithflag_t {
    AF_NULL,        // No flags
    AF_ROUND = 1,   // Round to # of decimal places
    AF_FLOOR = 128, // Floor result
    AF_CEIL  = 256  // Ceiling result
} arithflag_t;

// Printing options
typedef enum printflag_t {
    PF_NULL,        // No flags
    PF_SIGF = 1,    // Determines # of significant figures
    PF_FULL = 128,  // Print to full precision
    PF_SCIN = 256   // Always print in scientific notation
} printflag_t;

// Integral type used to store data within arbitrary-precision numbers
typedef uintmax_t bitfield_t;

// Arbitrary-precision integer
typedef struct dint_t {
    bitfield_t *bits;
    size_t size;
} dint_t;

// Arbitrary-precision floating-point number
typedef struct dflt_t {
    bitfield_t *whole, *decimal;
    size_t wsize, dsize;
} dflt_t;

import extern const dint_t *dint_one;
import extern const dint_t *dint_zero;

BEGIN

// ---- dflt_t ----

/* Takes:   void
 * Returns: dflt_t * (rvalue)
 *
 * Returns pointer to heap-allocated floating-point number
 * Returns NULL and sets errno on internal error */
#define dflt_new()  dflt_init(malloc(sizeof(dflt_t)))

/* Frees memory within a floating-point number
 * Does NOT free the number itself
 * Numbers allocated on the heap must still be freed afterward */
import void dflt_clr(dflt_t *val) nonnull noexcept;

/* Generates a copy of a floating-point number
 * Returns NULL and sets errno on internal error */
import dflt_t *dflt_cpy(const dflt_t *restricted val) nonnull noexcept queue;

// ---- dint_t ----

// -- Basic Utilities --

/* Frees contents within an integer
 * Must be called before free'ing if manually allocated */
import void dint_clr(dint_t *restricted val) nonnull noexcept;

/* Compares two integers
 * Returns NULL and sets errno on internal error
 *
 * Case            Return
 *  val1 < val2     -1
 *  val1 > val2      1
 *  val1 = val2      0 */
import int dint_cmp(const dint_t *val1, const dint_t *val2) nonnull noexcept pure;

/* Generates a copy of an integer
 * Returns NULL and sets errno on internal error */
import dint_t *dint_cpy(const dint_t *restricted val) nonnull noexcept queue;

/* Assigns value to target
 * Returns NULL and sets errno on internal error */
import dint_t *dint_eq(dint_t *target, const dint_t *val) nonnull noexcept;
import dint_t *dint_eqdf(dint_t *target, const dflt_t *val) nonnull noexcept;
import dint_t *dint_eqs(dint_t *target, intmax_t val) nonnull noexcept;
import dint_t *dint_equ(dint_t *target, uintmax_t val) nonnull noexcept;

/* Readies integer for use
 * Functions with two arguments assign an initial value to target
 * Otherwise, value of target is set to 0
 * Returns NULL and sets errno on internal error */
import dint_t *dint_init(dint_t *target) nonnull noexcept;
import dint_t *dint_initdf(dint_t *target, const dflt_t *val) nonnull noexcept;
import dint_t *dint_initdi(dint_t *target, const dint_t *val) nonnull noexcept;
import dint_t *dint_inits(dint_t *target, intmax_t val) nonnull noexcept;
import dint_t *dint_initu(dint_t *target, uintmax_t val) nonnull noexcept;

// Allocates automatically-free'd memory as well
#define dint_new()      dint_init(coll_dqueue(malloc(sizeof(dint_t)), dint_clr))
#define dint_newdf(val) dint_initdf(coll_dqueue(malloc(sizeof(dint_t)), dint_clr), val)
#define dint_newdi(val) dint_initdi(coll_dqueue(malloc(sizeof(dint_t)), dint_clr), val)
#define dint_news(val)  dint_inits(coll_dqueue(malloc(sizeof(dint_t)), dint_clr), val)
#define dint_newu(val)  dint_initu(coll_dqueue(malloc(sizeof(dint_t)), dint_clr), val)

// Returns true if integer is negative
import bool dint_isneg(const dint_t *val) nonnull noexcept;

// -- Basic Arithmetic --

/* Returns result of arithmetic/bitwise operation on two integers
 * Functions ending in '-eq' store result within target
 * All other functions store result within heap-allocated integer copy
 * Copies returned by such functions are automatically queued for deletion
 * All macro definitions are guaranteed to cause no side-effects
 * Returns NULL and sets errno on internal error */
#define dint_add(val1, val2)    dint_addeq(dint_cpy(val1), val2)
#define dint_and(val1, val2)    dint_andeq(dint_cpy(val1), val2)
#define dint_neg(val)           dint_negeq(dint_cpy(val))
#define dint_negeq(target)      dint_addeq(dint_noteq(target), dint_one)
#define dint_or(val1, val2)     dint_oreq(dint_cpy(val1), val2)
#define dint_not(val)           dint_noteq(dint_cpy(val))
#define dint_sub(val1, val2)    dint_subeq(dint_cpy(val1), val2)
#define dint_subeq(target, val) dint_addeq(target, dint_neg(val))
#define dint_xor(val1, val2)    dint_xoreq(dint_cpy(val1), val2)

import dint_t *dint_abs(const dint_t *val) nonnull noexcept;
import dint_t *dint_abseq(dint_t *target) nonnull noexcept;
import dint_t *dint_addeq(dint_t *target, const dint_t *val) nonnull noexcept;
import dint_t *dint_andeq(dint_t *target, const dint_t *val) nonnull noexcept;
import dint_t *dint_modeq(dint_t *target, const dint_t *val) nonnull noexcept;
import dint_t *dint_noteq(dint_t *target) nonnull noexcept;
import dint_t *dint_oreq(dint_t *target, const dint_t *val) nonnull noexcept;
import dint_t *dint_xoreq(dint_t *target, const dint_t *val) nonnull noexcept;

// Nonwhole results are truncated
#define dint_div(val1, val2)   dint_diveq(dint_cpy(val1), val2)
#define dint_mul(val1, val2)   dint_muleq(dint_cpy(val1), val2)

import dint_t *dint_diveq(dint_t *target, const dint_t *val) nonnull noexcept;
import dint_t *dint_muleq(dint_t *target, const dint_t *val) nonnull noexcept;

/* Bits shifted right out-of-bounds will be saved
 * Bits shifted left out-of-bounds, however, will not be */
#define dint_lshift(val, shift) dint_lshifteq(dint_cpy(val), shift)
#define dint_rshift(val, shift) dint_rshifteq(dint_cpy(val), shift)

import dint_t *dint_lshifteq(dint_t *target, size_t shift) nonnull noexcept;
import dint_t *dint_rshifteq(dint_t *dynint, size_t shift) nonnull noexcept;

END

#include <ladle/common/end_header.h>
#endif  // #ifndef LADLE_ARBITRARY_H
