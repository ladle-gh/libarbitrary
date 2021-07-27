#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "arbitrary.h"

// Allows for null check in spite of 'nonnull' attribute
#pragma GCC diagnostic ignored  "-Wnonnull-compare"

// ---- Basic API ----

/* Error handler
 * For use within an if-statement
 * Preserves original cause of error */
#define error(err, ret) {   \
    if (!errno)             \
        errno = err;        \
    return ret;             \
}

// ---- dynfloat_t Macros ----

/* Takes:   dynfloat_t *, size_t
 * Returns: bitfield_t * (rvalue)
 * 
 * Reallocates memory within whole buffer
 * Returns pointer to reallocated buffer */
#define dynfloat_wrealloc(value, resize)        \
    ((value)->whole = realloc((value)->whole,   \
    ((value)->wsize = (resize)) * sizeof(bitfield_t)))

/* Takes:   dynfloat_t *, size_t
 * Returns: bitfield_t * (rvalue)
 *
 * Reallocates memory within decimal buffer
 * Returns pointer to reallocated buffer */
#define dynfloat_drealloc(value, resize)            \
    ((value)->decimal = realloc((value)->decimal,   \
    ((value)->dsize = (resize)) * sizeof(bitfield_t)))

// ---- dynint_t Macros ----

/* Takes:   dynint_t *
 * Returns: bitfield_t &
 *
 * Returns final bitfield in bit buffer (lvalue)
 * Causes side-effects */
#define dynint_final(value) ((value)->bits[(value)->size - 1])

/* Takes:   dynint_t *, dynint_t *
 * Returns: dynint_t * (lvalue)
 *
 * Returns integer of largest bitfield size
 * Causes side-effects */
#define dynint_maxsz(value1, value2)    \
    ((value1)->size > (value2)->size ? (value1) : (value2))


/* Takes:   dynint_t *, dynint_t *
 * Returns: dynint_t * (lvalue)
 *
 * Returns integer of smallest bitfield size
 * Causes side-effects */
#define dynint_minsz(value1, value2)    \
    ((value1)->size < (value2)->size ? (value2) : (value1))

/* Takes:   dynint_t *, size_t
 * Returns: bitfield_t * (rvalue)
 *
 * Reallocates memory within bit buffer
 * Returns pointer to reallocated buffer
 * Causes side-effects */
#define dynint_realloc(value, resize)       \
    ((value)->bits = realloc((value)->bits, \
    ((value)->size = (resize)) * sizeof(bitfield_t)))

/* Takes:   dynint_t *
 * Returns: bool    (rvalue)
 *
 * Returns true if integer is negative
 * Causes side-effects */
#define dynint_sign(value) ((value)->bits[(value)->size - 1] & SIGN_BIT)

/* Takes:   dynint_t *
 * Returns: bitfield_t  (lvalue)
 *
 * Returns value dereferenced by iterator pointing to unallocated value */
#define dynint_unalloc(value)   (BITFLD_MAX * dynint_sign(value))   

// ---- iterator_t Macros ----

/* Takes:   iterator_t
 * Returns: bitfield_t (rvalue)
 *
 * Dereferences iterator
 * Returns BITFLD_MAX * sign if current index is out-of-range */
#define iter_peek(iter, where, size, sign)    \
    ((where) < (size) ? *(iter) : BITFLD_MAX * sign)

/* Takes:   iterator_t
 * Returns: bitfield_t (lvalue)
 *
 * Dereferences and increments iterator */
#define iter_next(iter) (*((iter)++))

/* Takes:   iterator_t
 * Returns: bitfield_t (lvalue)
 *
 * Dereferences and decrements iterator */
#define iter_rnext(iter)    (*((iter)--))

/* Takes:   iterator_t, size_t, size_t
 * Returns: bitfield_t (rvalue)
 *
 * Dereferences and increments iterator
 * Returns BITFLD_MAX * sign if current index is out-of-range */
#define iter_nextif(iter, where, size, sign)    \
    ((where) < (size) ? iter_next(iter) : BITFLD_MAX * sign)

/* Takes:   iterator_t, size_t, size_t
 * Returns: bitfield_t (rvalue)
 *
 * Dereferences and decrements iterator
 * Returns BITFLD_MAX * sign if current index is out-of-range */
#define iter_rnextif(iter, where, size, sign)   \
    ((where) < (size) ? iter_next(iter) : BITFLD_MAX * sign)

/* Takes:   iterator_t, size_t, size_t
 * Returns: bitfield_t (rvalue)
 *
 * Dereferences and increments iterator
 * Ignores sign bit */
#define iter_get(iter, where, size)     \
    (iter_next(iter) & (~SIGN_BIT | SIGN_BIT * ((where) != (size) - 1)))

/* Takes:   iterator_t, size_t, size_t
 * Returns: bitfield_t (rvalue)
 *
 * Dereferences and decrements iterator
 * Ignores sign bit */
#define iter_rget(iter, where, size)    \
    (iter_rnext(iter) & (~SIGN_BIT | SIGN_BIT * ((where) != (size) - 1)))

/* Takes:   iterator_t, size_t, size_t
 * Returns: bitfield_t (rvalue)
 *
 * Dereferences and increments iterator
 * Returns BITFLD_MAX * sign if current index is out-of-range
 * Ignores sign bit
 * Causes side-effects */
#define iter_getif(iter, where, size, sign)   \
    ((where) < (size) ? iter_get(iter, where, size) : BITFLD_MAX * sign)

/* Takes:   iterator_t, size_t, size_t
 * Returns: bitfield_t (rvalue)
 *
 * Dereferences and decrements iterator
 * Returns BITFLD_MAX * sign if current index is out-of-range
 * Ignores sign bit
 * Causes side-effects */
#define iter_rgetif(iter, where, size, sign)  \
    ((where) < (size) ? iter_rget(iter, where, size) : BITFLD_MAX * sign)

// ---- Constants ----

// Maximum number of bitfields an individual bitfield buffer may hold
#define BITFLD_CT_MAX   (SIZE_MAX / sizeof(bitfield_t))

// Maximum value of a bitfield
#define BITFLD_MAX      UINTMAX_MAX

// For last field in an integer or float, the position of the sign bit
#define SIGN_BIT        ((bitfield_t) 1 << sizeof(bitfield_t) * 8 - 1)

typedef bitfield_t *iterator_t;

typedef struct dyninfo_t {
    const dynint_t *integer;
    const size_t size;
} dyninfo_t;

// ---- dynfloat_t Functions ----
// TODO finish implementation

void dynfloat_free(dynfloat_t *value) {
    free(value->whole);
    free(value->decimal);
    free(value);
}
dynfloat_t *dynfloat_copy(const dynfloat_t *restrict value) {
    if (!value)
        error(EINVAL, NULL);

    dynfloat_t *copy = malloc(sizeof(dynfloat_t));

    if (!copy) // malloc() fails
        return NULL;
    copy->whole = malloc(value->wsize * sizeof(bitfield_t));
    copy->decimal = malloc(value->dsize * sizeof(bitfield_t));
    if (copy->whole || copy->decimal)  // malloc() fails
        return NULL;
    copy->wsize = value->wsize;
    copy->dsize = value->dsize;
    return copy;
}
dynfloat_t *dynfloat_new(void) {
    dynfloat_t *new_float = malloc(sizeof(dynfloat_t));

    if (!new_float)    // malloc() fails
        return NULL;
    new_float->whole =
      calloc(new_float->wsize = 2, 2 * sizeof(bitfield_t));
    new_float->decimal =
      calloc(new_float->dsize = 2, 2 * sizeof(bitfield_t));
    if (!new_float->whole || !new_float->decimal)  // calloc() fails
        return NULL;
    return new_float;
}

// ---- dynint_t Functions ----
// TODO finish implementation

/* Returns struct containing information
 * Pertains to the integer with the most significant bits */
static const dynint_t *dynint_maxsig(const dynint_t *value1, const dynint_t *value2) {
    const bool sign1 = dynint_sign(value1), sign2 = dynint_sign(value2);
    const size_t size1 = value1->size, size2 = value2->size;
    const bitfield_t unalloc1 = BITFLD_MAX * sign1, unalloc2 = BITFLD_MAX * sign2;
    iterator_t iter1 = value1->bits, iter2 = value2->bits;

    for (size_t i = 0;; ++i) {
        if (iter_nextif(iter1, i, size1, sign1) == unalloc1)
            return value2;
        if (iter_nextif(iter2, i, size2, sign2) == unalloc2)
            return value1;
    }
}

/* Returns struct containing information
 * Pertains to the integer with the least significant bits */
static const dynint_t *dynint_minsig(const dynint_t *value1, const dynint_t *value2) {
    const bool sign1 = dynint_sign(value1), sign2 = dynint_sign(value2);
    const size_t size1 = value1->size, size2 = value2->size;
    const bitfield_t unalloc1 = BITFLD_MAX * sign1, unalloc2 = BITFLD_MAX * sign2;
    iterator_t iter1 = value1->bits, iter2 = value2->bits;

    for (size_t i = 0;; ++i) {
        if (iter_nextif(iter1, i, size1, sign1) == unalloc1)
            return value1;
        if (iter_nextif(iter2, i, size2, sign2) == unalloc2)
            return value2;
    }
}

dynint_t *dynint_add(const dynint_t *value1, const dynint_t *value2) {
    if (!value1 || !value2)
        error(EINVAL, NULL);
    return dynint_addeq(dynint_copy(dynint_maxsig(value1, value2)),
      dynint_minsig(value1, value2));
}
#include <stdio.h>
#include <numsys.h>
dynint_t *dynint_addeq(dynint_t *target, const dynint_t *value) {
    if (!target || !value)
        error(EINVAL, NULL);

    const dynint_t *max_sig = dynint_maxsig(target, value);

    // Append bitfield as overflow buffer, if needed
    if (target->size < max_sig->size + (dynint_final(max_sig) != dynint_unalloc(max_sig))
      && !dynint_realloc(target, max_sig->size))
        return NULL;    // dynint_realloc() fails

    bool swp, overflow = false;
    const bool target_sign = dynint_sign(value), val_sign = dynint_sign(value);
    const size_t target_sz = target->size, val_sz = value->size;
    iterator_t target_iter = target->bits, val_iter = value->bits;

    for (size_t i = 0, lim = target->size; i < lim; ++i) {
        swp = *target_iter > BITFLD_MAX - *val_iter ||
          *val_iter > BITFLD_MAX - *target_iter;

        /* "A computation involving unsigned operands can never overflow"
         *  -- C11 Standard, 6.2.5 (9) */

        //## BEGIN DEBUG ##
        printf(
          "%s + %s (swp = %d, overflow = %d) =\n%lu + %lu =\n",
          numsys_utostring(*target_iter, 2), numsys_utostring(*val_iter, 2), swp, overflow, *target_iter, *val_iter
        );
        //### END DEBUG ###

        *target_iter = (*target_iter & (~SIGN_BIT | SIGN_BIT * (i != target_sz - 1))) + iter_nextif(val_iter, i, val_sz, val_sign) + overflow;

        //## BEGIN DEBUG ##
        printf("%s\n%lu\n\n", numsys_utostring(*target_iter, 2), *target_iter);
        //### END DEBUG ###

        ++target_iter;
        overflow = swp;
    }
    return target;
}
dynint_t *dynint_abs(const dynint_t *restrict value) {
    if (!value)
        error(EINVAL, NULL);
    return dynint_abseq(dynint_copy(value));    // Handles dynint_copy() failure
}
dynint_t *dynint_abseq(dynint_t *value) {
    if (!value)
        error(EINVAL, NULL);
    return dynint_sign(value) ? dynint_negeq(value) : value;
}
dynint_t *dynint_and(const dynint_t *value1, const dynint_t *value2) {
    if (!value1 || !value2)
        error(EINVAL, NULL);
    return dynint_andeq(dynint_copy(dynint_minsz(value1, value2)),
      dynint_maxsz(value1, value2));
}
dynint_t *dynint_andeq(dynint_t *target, const dynint_t *value) {
    if (!target || !value)
        error(EINVAL, NULL);

    iterator_t target_iter = target->bits, val_iter = value->bits;

    for (size_t i = 0, lim = dynint_minsz(target, value)->size; i < lim; ++i)
        iter_next(target_iter) |= iter_next(val_iter);
    return target; 
}
int dynint_cmp(const dynint_t *value1, const dynint_t *value2) {    // TODO finish
    if (!value1 || !value2)
        error(EINVAL, 2);

    const bool sign1 = dynint_sign(value1), sign2 = dynint_sign(value2);

    if (sign1 ^ sign2)
        return sign1 ? -1 : 1;

    int cmp_val;
    const size_t final = dynint_maxsz(value1, value2)->size - 1,
      size1 = value1->size, size2 = value2->size;
    iterator_t iter1 = value1->bits + final, iter2 = value2->bits + final;

    for (size_t i = final;; --i) {
        cmp_val =
            (iter_peek(iter1, i, size1, sign1) & ~SIGN_BIT >
              iter_peek(iter2, i, size2, sign2) & ~SIGN_BIT) -
            (iter_getif(iter1, i, size1, sign1) < iter_getif(iter2, i, size2, sign2));
        if (!cmp_val)   return cmp_val;
        if (!i)         return 0;
    }
}
dynint_t *dynint_copy(const dynint_t *restrict value) {
    if (!value)
        error(EINVAL, NULL);

    dynint_t *copy = malloc(sizeof(dynint_t));

    if (!copy) // malloc() fails
        return NULL;
    copy->bits = malloc(value->size * sizeof(bitfield_t));
    if (!copy->bits)    // malloc() fails
        return NULL;
    memcpy(copy->bits, value->bits, value->size * sizeof(bitfield_t));
    copy->size = value->size;
    return copy;
}
#define dynint_clear(value) ((value) ? free((value)->bits) : NULL)
dynint_t *dynint_init(dynint_t *target) {
    if (!target)
        error(EINVAL, NULL);
    target->bits = calloc(target->size = 2, 2 * sizeof(bitfield_t));
    if (!target->bits)    // calloc() fails
        return NULL;
    return target;
}
dynint_t *dynint_initi(dynint_t *target, bitfield_t bits) {
    if (!target)
        error(EINVAL, NULL);
    target->bits = malloc((target->size = 2) * sizeof(bitfield_t));
    if (!target->bits)    // malloc() fails
        return NULL;
    target->bits[1] = BITFLD_MAX * !!(bits & SIGN_BIT);
    target->bits[0] = bits;
    return target;
}
dynint_t *dynint_initui(dynint_t *target, bitfield_t bits) {
    if (!target)
        error(EINVAL, NULL);
    target->bits = malloc((target->size = 2) * sizeof(bitfield_t));
    if (!target->bits)    // malloc() fails
        return NULL;
    target->bits[1] = 0;
    target->bits[0] = bits;
    return target;
}
dynint_t *dynint_initdi(dynint_t *target, const dynint_t *integer) {
    if (!target || !integer)
        error(EINVAL, NULL);
}
dynint_t *dynint_initdf(dynint_t *target, const dynfloat_t *floating) {
    if (!target || !floating)
        error(EINVAL, NULL);
}
#define dynint_new()    dynint_init(malloc(sizeof(dynint_t)))
dynint_t *dynint_not(const dynint_t *value) {
    if (!value)
        error(EINVAL, NULL);
    return dynint_noteq(dynint_copy(value));
}
dynint_t *dynint_noteq(dynint_t *target) {
    if (!target)
        error(EINVAL, NULL);

    iterator_t iter = target->bits;

    for (size_t i = 0, lim = target->size; i < lim; ++i) {

        /* "The evaluations of the operands are unsequenced."
         *  --  C11 Standard, 6.5.16 (3) */
        *iter = ~(*iter);
        ++iter;
        
    }
    return target;
}
dynint_t *dynint_or(const dynint_t *value1, const dynint_t *value2) {
    if (!value1 || !value2)
        error(EINVAL, NULL);
    return dynint_oreq(dynint_copy(dynint_maxsz(value1, value2)),
      dynint_minsz(value1, value2));
}
dynint_t *dynint_oreq(dynint_t *target, const dynint_t *value) {
    if (!target || !value)
        error(EINVAL, NULL);
    if (target->size < value->size && !dynint_realloc(target, value->size))
        return NULL;    // dynint_realloc() fails

    const bool val_sign = dynint_sign(value);
    const size_t val_sz = value->size;
    iterator_t target_iter = target->bits, val_iter = value->bits;

    for (size_t i = 0, lim = target->size; i < lim; ++i)
        iter_next(target_iter) |= iter_nextif(val_iter, i, val_sz, val_sign);
    return target; 
}
dynint_t *dynint_sub(const dynint_t *value1, const dynint_t *value2) {
    if (!value1 || !value2)
        error(EINVAL, NULL);
    return dynint_subeq(dynint_copy(dynint_maxsig(value1, value2)),
      dynint_minsig(value1, value2));
}
dynint_t *dynint_subeq(dynint_t *target, const dynint_t *value) {
    if (!target || !value)
        error(EINVAL, NULL);

    dynint_t *copy = dynint_negeq(dynint_copy(value));

    if (!copy)  // dynint_copy() fails
        return NULL;
    dynint_addeq(target, copy);
    dynint_clear(copy);
    free(copy);
    return target;
}
dynint_t *dynint_xor(const dynint_t *value1, const dynint_t *value2) {
    if (!value1 || !value2)
        error(EINVAL, NULL);
    return dynint_xoreq(dynint_copy(dynint_maxsz(value1, value2)),
      dynint_minsz(value1, value2));
}
dynint_t *dynint_xoreq(dynint_t *target, const dynint_t *value) {
    if (!target || !value)
        error(EINVAL, NULL);
    if (target->size < value->size && !dynint_realloc(target, value->size))
        return NULL;    // dynint_realloc() fails

    const bool val_sign = dynint_sign(value);
    const size_t val_sz = value->size;
    iterator_t target_iter = target->bits, val_iter = value->bits;

    for (size_t i = 0, lim = target->size; i < lim; ++i)
        iter_next(target_iter) ^= iter_nextif(val_iter, i, val_sz, val_sign);
    return target;
}

// ---- Testing ----

#include <stdio.h>
#include <numsys.h>

int main(void) {
    dynint_t num1, num2;
    dynint_initi(&num1, INTMAX_MIN +1);
    dynint_initi(&num2, -5555);

    dynint_addeq(&num1, &num2);
    for (size_t i = num1.size - 1;; --i) {
        printf("%s ", numsys_utostring(num1.bits[i], 2));
        if (!i)
            break;
    }
    dynint_clear(&num1);
    dynint_clear(&num2);
    return 0;
}