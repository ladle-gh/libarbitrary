#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <ladle/collect.h>
#include <ladle/common/lib.h>
#include <ladle/common/ptrcmp.h>

#include "arbitrary.h"

/* Takes:   T, T; T is an integral type
 * Returns: T
 *
 * Ceiling division
 * Causes side-effects */
#define ceil_div(num, denom)  ((num) / (denom) + !!((num) % (denom)))

// ---- dflt_t Macros ----

/* Takes:   dflt_t *, size_t
 * Returns: bitfield_t * (rvalue)
 * 
 * Reallocates memory within whole buffer
 * Returns pointer to reallocated buffer */
#define dflt_wrealloc(val, resize)        \
    ((val)->whole = realloc((val)->whole,   \
    ((val)->wsize = (resize)) * sizeof(bitfield_t)))

/* Takes:   dflt_t *, size_t
 * Returns: bitfield_t * (rvalue)
 *
 * Reallocates memory within decimal buffer
 * Returns pointer to reallocated buffer */
#define dflt_drealloc(val, resize)            \
    ((val)->decimal = realloc((val)->decimal,   \
    ((val)->dsize = (resize)) * sizeof(bitfield_t)))

// ---- dint_t Macros ----

/* Takes:   dint_t *
 * Returns: bitfield_t (lvalue)
 *
 * Returns final bitfield in bit buffer (lvalue)
 * Causes side-effects */
#define dint_final(val) ((val)->bits[(val)->size - 1])

/* Takes:   T, where T is a signed integral
 * Returns: dint_t * (rvalue)
 *
 * Creates a local instance of an integer from a signed integral val
 * Returns pointer of result */
#define dint_fromsi(signed_int)   (&(dint_t) {(bitfield_t[]) {signed_int}, 1})

/* Takes:   T, where T is an unsigned integral
 * Returns: dint_t * (rvalue)
 *
 * Creates a local instance of an integer from an unsigned integral val
 * Returns pointer of result */
#define dint_fromui(unsigned_int) (&(dint_t) {(bitfield_t[]) {0, unsigned_int}, 2})

/* Takes:   dint_t *, dint_t *
 * Returns: dint_t * (rvalue)
 *
 * Returns integer of largest bitfield size
 * Causes side-effects */
#define dint_maxsz(val1, val2)    \
    ((val1)->size > (val2)->size ? (val1) : (val2))


/* Takes:   dint_t *, dint_t *
 * Returns: dint_t * (rvalue)
 *
 * Returns integer of smallest bitfield size
 * Causes side-effects */
#define dint_minsz(val1, val2)    \
    ((val1)->size < (val2)->size ? (val2) : (val1))

/* Takes:   dint_t *
 * Returns: bitfield_t (rvalue)
 *
 * Returns val dereferenced by iterator pointing to unallocated val */
#define dint_unalloc(val)   (BITFLD_MAX * dint_isneg(val))   

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
    ((where) < (size) ? iter_next(iter) : BITFLD_MAX * sign + !(++iter))

/* Takes:   iterator_t, size_t, size_t
 * Returns: bitfield_t (rvalue)
 *
 * Dereferences and decrements iterator
 * Returns BITFLD_MAX * sign if current index is out-of-range */
#define iter_rnextif(iter, where, size, sign)   \
    ((where) < (size) ? iter_rnext(iter) : BITFLD_MAX * sign * !(--iter))

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
    ((where) < (size) ? iter_get(iter, where, size) : BITFLD_MAX * sign * !(++iter))

/* Takes:   iterator_t, size_t, size_t
 * Returns: bitfield_t (rvalue)
 *
 * Dereferences and decrements iterator
 * Returns BITFLD_MAX * sign if current index is out-of-range
 * Ignores sign bit
 * Causes side-effects */
#define iter_rgetif(iter, where, size, sign)  \
    ((where) < (size) ? iter_rget(iter, where, size) : BITFLD_MAX * sign * !(--iter))

// ---- Constants ----

/* Number of bits within a bitfield */
#define BITFLD_BITS     (sizeof(bitfield_t) * 8)

/* Maximum number of bitfields a bit buffer may hold */
#define BITFLD_CT_MAX   (SIZE_MAX / (sizeof(bitfield_t) * 8))

/* Maximum val of a bitfield */
#define BITFLD_MAX      UINTMAX_MAX

/* Maximum number of bits stored within bit buffer */
#define MAX_BITS        SIZE_MAX

/* For last field in an integer or float, the position of the sign bit */
#define SIGN_BIT        ((bitfield_t) 1 << sizeof(bitfield_t) * 8 - 1)

typedef bitfield_t *iterator_t;

typedef struct dyninfo_t {
    const dint_t *integer;
    const size_t size;
} dyninfo_t;

export const dint_t *dint_maxbits = &(dint_t) {(bitfield_t[]) {0, MAX_BITS}, 2};
export const dint_t *dint_one = &(dint_t) {(bitfield_t[]) {1}, 1};
export const dint_t *dint_zero = &(dint_t) {(bitfield_t[]) {0}, 1};

// ---- dflt_t Functions ----

void dflt_clr(dflt_t *val) {
    free(val->whole);
    free(val->decimal);
}
dflt_t *dflt_cpy(const dflt_t *restrict val) {
    if (!val)
        error(EINVAL, NULL);

    dflt_t *copy = malloc(sizeof(dflt_t));

    if (!copy) // malloc() fails
        return NULL;
    copy->whole = malloc(val->wsize * sizeof(bitfield_t));
    copy->decimal = malloc(val->dsize * sizeof(bitfield_t));
    if (copy->whole || copy->decimal)  // malloc() fails
        return NULL;
    copy->wsize = val->wsize;
    copy->dsize = val->dsize;
    return copy;
}

// ---- dint_t ----

// -- Static --

// Returns integer with most significant bits
static const dint_t *dint_maxsig(const dint_t *val1, const dint_t *val2) {
    const dint_t *maxsz = dint_maxsz(val1, val2);
    const dint_t *min = ptr_sub(ptr_add(val1, val2), maxsz), *max = maxsz;
    iterator_t min_iter = min->bits + (min->size - 1),
      max_iter = max->bits + (max->size - 1);

    for (size_t i = max->size - 1, lim = min->size; i >= lim; --i) {
        if (iter_rnext(max_iter))
            return max == val1 ? val1 : val2;
    }
    for (size_t i = min->size - 1;; --i) {
        if (*max_iter > *min_iter)
            return max == val1 ? val1 : val2;
        if (iter_rnext(max_iter) < iter_rnext(min_iter))
            return min == val1 ? val1 : val2;
        if (!i)
            return val1;
    }
}

// Returns integer with least significant bits
static const dint_t *dint_minsig(const dint_t *val1, const dint_t *val2) {
    const dint_t *maxsz = dint_maxsz(val1, val2);
    const dint_t *min = ptr_sub(ptr_add(val1, val2), maxsz), *max = maxsz;
    iterator_t min_iter = min->bits + (min->size - 1),
      max_iter = max->bits + (max->size - 1);

    for (size_t i = max->size - 1, lim = min->size; i >= lim; --i) {
        if (iter_rnext(max_iter))
            return min == val1 ? val1 : val2;
    }
    for (size_t i = min->size - 1;; --i) {
        if (*max_iter > *min_iter)
            return min == val1 ? val1 : val2;
        if (iter_rnext(max_iter) < iter_rnext(min_iter))
            return max == val1 ? val1 : val2;
        if (!i)
            return val2;  // Different result for equals
    }
}

// Returns padding between most significant 1 bits and end of bitfield, in bits
static unsigned bitfld_pad(bitfield_t bits) {
    unsigned ct = 0;

    while (bits) {
        bits >>= 1;
        ++ct;
    }
    return BITFLD_BITS - ct;
}

// Returns padding between most significant 1 bit and end of bit buffer, in bits
static size_t dint_pad(const dint_t *val) {
    iterator_t iter = val->bits + (val->size - 1);

    for (size_t i = 0, lim = val->size; i < lim; ++i) {
        if (*iter)
            return i * BITFLD_BITS + bitfld_pad(*iter);
        --iter;
    }
    return val->size * BITFLD_BITS;    // Integer equals 0
}

static dint_t *dint_ext(dint_t *target, size_t resize) {
    if (resize > BITFLD_CT_MAX) // Integer too large
        error(ERANGE, NULL);

    const bool is_signed = dint_isneg(target);

    target->bits = realloc(target->bits, resize * sizeof(bitfield_t));
    if (!target->bits)  // realloc() fails
        return NULL;
    memset(target->bits + target->size, ~0 * is_signed, resize - target->size);
    target->size = resize;
    return target;
}

// -- Basic Utilities --

export int dint_cmp(const dint_t *val1, const dint_t *val2) {
    if (!val1 || !val2)
        error(EINVAL, 2);

    const bool sign1 = dint_isneg(val1), sign2 = dint_isneg(val2);

    if (sign1 ^ sign2)
        return sign1 ? -1 : 1;

    int cmpval;
    const size_t final = dint_maxsz(val1, val2)->size - 1,
      size1 = val1->size, size2 = val2->size;
    iterator_t iter1 = val1->bits + final, iter2 = val2->bits + final;

    for (size_t i = final;; --i) {
        cmpval =
            (iter_peek(iter1, i, size1, sign1) > iter_peek(iter2, i, size2, sign2)) -
            (iter_rnextif(iter1, i, size1, sign1) < iter_rnextif(iter2, i, size2, sign2));
        if (cmpval)    return sign1 ? -cmpval : cmpval;
        if (!i)         return 0;
    }
}
export void dint_clr(dint_t *restrict val) {
    if (val)
        free(val->bits);
    val->bits = NULL;   // Prevent double free
}
export dint_t *dint_cpy(const dint_t *restrict val) {
    if (!val)
        error(EINVAL, NULL);

    dint_t *copy = coll_dqueue(malloc(sizeof(dint_t)), dint_clr);

    if (!copy)  // malloc() fails
        return NULL;

    copy->bits = malloc(val->size * sizeof(bitfield_t));
    if (!copy->bits)    // malloc() fails
        return NULL;
    memcpy(copy->bits, val->bits, val->size * sizeof(bitfield_t));
    copy->size = val->size;
    return copy;
}
export dint_t *dint_eq(dint_t *target, const dint_t *val) {
    if (!target || !val)
        error(EINVAL, NULL);
    if (target->size < val->size) {
        free(target->bits);
        return dint_initdi(target, val);
    }
    memcpy(target->bits, val->bits, val->size * sizeof(bitfield_t));
    memset(target->bits + val->size, ~0 * dint_isneg(val),
      (target->size - val->size) * sizeof(bitfield_t));
    return target;
}
export dint_t *dint_eqdf(dint_t *target, const dflt_t *val) {
    if (!target || !val)
        error(EINVAL, NULL);
    // TODO finish implementation
}
export dint_t *dint_eqs(dint_t *target, intmax_t val) {
    if (!target)
        error(EINVAL, NULL);
    memcpy(target->bits, &val, sizeof(bitfield_t));
    memset(target->bits + 1, ~0 * (val < 0),
      (target->size - 1) * sizeof(bitfield_t));
    return target;
}
export dint_t *dint_equ(dint_t *target, uintmax_t val) {
    if (!target)
        error(EINVAL, NULL);
    memcpy(target->bits, &val, sizeof(bitfield_t));
    memset(target->bits + 1, 0, (target->size - 1) * sizeof(bitfield_t));
    return target;
}
export dint_t *dint_init(dint_t *target) {
    if (!target)
        error(EINVAL, NULL);
    target->bits = calloc(target->size = 2, 2 * sizeof(bitfield_t));
    if (!target->bits)  // calloc() fails
        return NULL;
    return target;
}
export dint_t *dint_initdf(dint_t *target, const dflt_t *val) {
    if (!target || !val)
        error(EINVAL, NULL);
    // TODO finish implementation
}
export dint_t *dint_initdi(dint_t *target, const dint_t *val) {
    if (!target || !val)
        error(EINVAL, NULL);
    target->bits = malloc((target->size = val->size) * sizeof(bitfield_t));
    if (!target->bits)  // malloc() fails
        return NULL;
    memcpy(target->bits, val->bits, val->size * sizeof(bitfield_t));
    return target;
}
export dint_t *dint_inits(dint_t *target, intmax_t val) {
    if (!target)
        error(EINVAL, NULL);
    target->bits = malloc((target->size = 2) * sizeof(bitfield_t));
    if (!target->bits)  // malloc() fails
        return NULL;
    target->bits[1] = BITFLD_MAX * !!(val & SIGN_BIT);
    target->bits[0] = val;
    return target;
}
export dint_t *dint_initu(dint_t *target, uintmax_t val) {
    if (!target)
        error(EINVAL, NULL);
    target->bits = malloc((target->size = 2) * sizeof(bitfield_t));
    if (!target->bits)  // malloc() fails
        return NULL;
    target->bits[1] = 0;
    target->bits[0] = val;
    return target;
}
export bool dint_isneg(const dint_t *val) {
    return val && val->bits[val->size - 1] & SIGN_BIT;
}

// -- Basic Arithmetic --

export dint_t *dint_abs(const dint_t *val) {
    return dint_isneg(val) ? dint_neg(val) : dint_cpy(val);
}
export dint_t *dint_abseq(dint_t *target) {
    return dint_isneg(target) ? dint_negeq(target) : target;
}
export dint_t *dint_addeq(dint_t *target, const dint_t *val) {
    if (!target || !val)
        error(EINVAL, NULL);

    const dint_t *maxsig = dint_maxsig(target, val);

    if (target->size < maxsig->size + (dint_final(maxsig) != dint_unalloc(maxsig))
      && !dint_ext(target, maxsig->size))   // Append bitfield as overflow, if needed
        return NULL;    // dint_ext() fails

    bool swp, overflow = false;
    const bool target_sign = dint_isneg(target), val_sign = dint_isneg(val);
    const size_t val_sz = val->size;
    iterator_t target_iter = target->bits, val_iter = val->bits;

    for (size_t i = 0, lim = target->size; i < lim; ++i) {
        swp = *target_iter > BITFLD_MAX - *val_iter ||
          *val_iter > BITFLD_MAX - *target_iter;
        *target_iter = *target_iter +
          iter_nextif(val_iter, i, val_sz, val_sign) + overflow;
        ++target_iter;
        overflow = swp;
    }
    return target;
}
export dint_t *dint_andeq(dint_t *target, const dint_t *val) {
    if (!target || !val)
        error(EINVAL, NULL);

    iterator_t target_iter = target->bits, val_iter = val->bits;

    for (size_t i = 0, lim = dint_minsz(target, val)->size; i < lim; ++i)
        iter_next(target_iter) |= iter_next(val_iter);
    return target; 
}
export dint_t *dint_noteq(dint_t *target) {
    if (!target)
        error(EINVAL, NULL);

    iterator_t iter = target->bits;

    for (size_t i = 0, lim = target->size; i < lim; ++i) {
        *iter = ~(*iter);
        ++iter;
    }
    return target;
}
export dint_t *dint_oreq(dint_t *target, const dint_t *val) {
    if (!target || !val)
        error(EINVAL, NULL);
    if (target->size < val->size && !dint_ext(target, val->size))
        return NULL;    // dint_ext() fails

    const bool val_sign = dint_isneg(val);
    const size_t val_sz = val->size;
    iterator_t target_iter = target->bits, val_iter = val->bits;

    for (size_t i = 0, lim = target->size; i < lim; ++i)
        iter_next(target_iter) |= iter_nextif(val_iter, i, val_sz, val_sign);
    return target; 
}
export dint_t *dint_xoreq(dint_t *target, const dint_t *val) {
    if (!target || !val)
        error(EINVAL, NULL);
    if (target->size < val->size && !dint_ext(target, val->size))
        return NULL;    // dint_ext() fails

    const bool val_sign = dint_isneg(val);
    const size_t val_sz = val->size;
    iterator_t target_iter = target->bits, val_iter = val->bits;

    for (size_t i = 0, lim = target->size; i < lim; ++i)
        iter_next(target_iter) ^= iter_nextif(val_iter, i, val_sz, val_sign);
    return target;
}
export dint_t *dint_muleq(dint_t *target, const dint_t *val) {
    if (!target || !val)
        error(EINVAL, NULL);
    if (!dint_cmp(target, dint_zero) || !dint_cmp(val, dint_zero))
        return dint_eq(target, dint_zero);

    const dint_t *maxsig = dint_maxsig(target, val);
    dint_t *min = dint_abs(ptr_sub(ptr_add(target, val), maxsig)),
      *max = dint_abs(maxsig);

    if (!min || !max)   // dint_abs() fails
        return NULL;
    if (dint_cmp(min, dint_maxbits) > 0)    // Shift overflow
        error(ERANGE, NULL);

    const bool negate = dint_isneg(target) ^ dint_isneg(val);

    if (dint_cmp(min, dint_one) && dint_cmp(max, dint_one)) {
        dint_t *buf = dint_cpy(max);

        if (min->bits[0])
            dint_lshifteq(max, min->bits[0] / 2);
        if (min->bits[0] & 1)   // Odd multiplicand
            max = dint_addeq(max, buf); // Assigns NULL on error
    }
    if (!dint_eq(target, max))  // dint_eq() fails
        return NULL;
    return negate ? dint_negeq(target) : target;    
}
export dint_t *dint_diveq(dint_t *target, const dint_t *val) {
    if (!target || !val)
        error(EINVAL, NULL);
    if (!dint_cmp(val, dint_zero))    // Denominator is 0
        error(EDOM, NULL);
    if (!dint_cmp(target, dint_zero))   // Numerator is 0
        return target;
    switch (dint_cmp(target, val)) {
    case -1:    return dint_eq(target, dint_zero);  // Numerator < Denominator
    case  0:    return dint_eq(target, dint_one);   // Numerator = Denominator
    }

    dint_t *target_abs = dint_abs(target), *val_abs = dint_abs(val);
    dint_t di_buf;

    if (!target_abs || !val_abs)    // dint_abs() fails
        return NULL;
    if (target->size <= BITFLD_CT_MAX / 2 &&
      !dint_initu(&di_buf, target->size * BITFLD_BITS * 2))  // dint_initu() fails
        return NULL;

    int cmpval = dint_cmp(val_abs, &di_buf);

    dint_clr(&di_buf);
    if (cmpval) // Right shift overflows entire integer
        return dint_eq(target, dint_zero);

    const bool negate = dint_isneg(target) ^ dint_isneg(val);

    if (dint_cmp(val_abs, dint_one)) {
        if (val_abs->bits[0] & 1) {    // Odd denominator
            const size_t val_bits = val_abs->size * 8 - dint_pad(val);
            size_t target_bits = target_abs->size * 8 - dint_pad(target_abs),
              rem_bits = target_bits - val_bits;
            dint_t remaining;

            remaining.size = ceil_div(rem_bits, BITFLD_BITS);
            remaining.bits = malloc(di_buf.size * sizeof(bitfield_t));
            if (!remaining.bits)    // malloc() fails
                return NULL;

            static const unsigned char *rev_lookup = {
              0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE,
              0x1, 0x9, 0x5, 0xD, 0x3, 0xb, 0x7, 0xF};
            char cur_byte;
            unsigned shift;
            bitfield_t cur_bitfld, rev_bitfld = 0;
            iterator_t target_iter = target_abs->bits + (target->size - 1);

            for (size_t i = target->size - 1;; --i) {
                if (!iter_rnext(target_iter)) {
                    shift = bitfld_pad(*(target_iter + 1));
                    break;
                }
            }
            for (size_t i = 0, lim = remaining.size; i < lim; ++i) {
                cur_bitfld = iter_rnext(target_iter);
                for (size_t i = 0, lim = BITFLD_BITS; i < lim; i += 8) {
                    cur_byte = cur_bitfld & (0xFFULL << i);
                    rev_bitfld |= (bitfield_t) rev_lookup[(cur_byte >> i) & 0xF]
                      << (lim - i - 1);
                }
                remaining.bits[i] = rev_bitfld;
            }
            if (!dint_rshifteq(target_abs, target_bits -= rem_bits)) {
                dint_clr(&remaining);
                return NULL;    // dint_rshifteq() fails
            }
            if (!dint_rshifteq(&remaining, shift)) {  // dint_rshifteq() fails
                dint_clr(&remaining);
                return NULL;
            }
            target_iter = target->bits;
            for (size_t i = 0, lim = target->size; i < lim; ++i) {  // Get number of bits
                if (iter_next(target_iter)) {
                    --target_iter;
                    target_bits = i * (*target_iter * 8) + bitfld_pad(*target_iter);
                }
            }
            for (size_t i = shift;; ++i) {
                /*  TODO finish

                    shift = target_bits - val_bits;
                    dint_lshifteq(target_abs, shift);
                    target_bits += val_bits
                    if (di_buf = dynint)
                    dint_and(remaining, dint_r)
                    dint_oreq(target_abs, *remaining & 1);
                    remaining += !!(i % 8);
                */
            }
            dint_clr(&remaining);
            dint_clr(&di_buf);
        } else
            target_abs = dint_rshifteq(target_abs, val_abs->bits[0] / 2);
        if (!dint_eq(target, target_abs))   // dint_eq() fails
            return NULL;
    }
    return negate ? dint_negeq(target) : target;
}
export dint_t *dint_lshifteq(dint_t *target, size_t shift) {
    if (!target)
        error(EINVAL, NULL);

    const size_t move = shift / BITFLD_BITS;
    const size_t padding = dint_pad(target);

    if (shift > padding) {
        size_t add = (shift - padding) / BITFLD_BITS;

        add += !!((shift - padding) % BITFLD_BITS);
        if (target->size > BITFLD_CT_MAX - add - 1) // Result too large
            error(ERANGE, NULL);
        if (!dint_ext(target, target->size + add))
            return NULL;    // dint_ext() fails
    }

    bitfield_t carry = 0, swp;
    iterator_t iter = target->bits + move;

    shift %= BITFLD_BITS;
    memmove(target->bits + move, target->bits, (target->size - move) * sizeof(bitfield_t));
    memset(target->bits, 0, move * sizeof(bitfield_t));
    for (size_t i = move, lim = target->size; i < lim; ++i) {
        swp = (*iter & ~(BITFLD_MAX >> shift)) >> BITFLD_BITS - shift ; // Get carry
        *iter = (*iter << shift) | carry;   // Apply carry
        carry = swp;
        ++iter;
    }
    return target;
}
export dint_t *dint_rshifteq(dint_t *target, size_t shift) {
    if (!target)
        error(EINVAL, NULL);

    const size_t move = shift / BITFLD_BITS;

    if (move >= target->size)
        return dint_eq(target, dint_zero);

    bitfield_t carry = 0, swp;
    iterator_t iter = target->bits + (target->size - 1);

    shift %= BITFLD_BITS;
    memmove(target->bits, target->bits + move, (target->size - move) * sizeof(bitfield_t));
    memset(target->bits + (target->size - move), 0, move * sizeof(bitfield_t));
    for (size_t i = target->size - move - 1;; --i) {
        swp = (*iter & ~(BITFLD_MAX << shift)) << BITFLD_BITS - shift ; // Get carry
        *iter = (*iter >> shift) | carry;   // Apply carry
        carry = swp;
        --iter;
        if (!i)
            return target;
    }
}
