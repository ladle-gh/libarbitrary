#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <ladle/common/lib.h>
#include <ladle/common/ptrcmp.h>

#include "arbitrary.h"
#include "etc.h"

#define PREFIX  dwhl

// TODO find EVERY instance of a possible `rvalue' integer being used in implementation function
// Code workaround so rvalue can be used without freeing itself

// ---- Constants ----

// Number of bits within a bitfield
#define BITFLD_BITS     (sizeof(bitfld_t) * 8)

/* Maximum number of bitfields a bit buffer may hold
 * Ensures # of total bits representable as shift_t */
#define BITFLD_CT_MAX   (SHIFT_MAX / (sizeof(bitfld_t) * 8))

// Maximum val of a bitfield
#define BITFLD_MAX      UINTMAX_MAX

// For last field in an integer or float, the position of the sign bit
#define SIGN_BIT        ((bitfld_t) 1 << sizeof(bitfld_t) * 8 - 1)

// Convenience constants
export const dwhl_t *const dwhl_one  = &(dwhl_t) {(bitfld_t[]) {1}, 1, false};
export const dwhl_t *const dwhl_zero = &(dwhl_t) {(bitfld_t[]) {0}, 1, false};

// Maximum value to be multiplied by
static dwhl_t *maxmul = &(dwhl_t) {/* TODO find value */};

// ---- Helper Functions ----

static dwhl_t *do_div(dwhl_t *, const dwhl_t *, bool);
static dwhl_t *do_lshift(dwhl_t *, shift_t, bitfld_t);
static dwhl_t *do_mul(dwhl_t *, dwhl_t *);
static dwhl_t *extend(dwhl_t *tar, size_t resize);
static dwhl_t *max_sig(const dwhl_t *, const dwhl_t *);
static shift_t padding(const dwhl_t *);
static shift_t sig_bits(const dwhl_t *);

static inline void clr_rval(const dwhl_t *, bool);
static inline bool get_bit(const dwhl_t *, shift_t);
static inline bool is_rval(const dwhl_t *);
static inline bitfld_t insig_val(const dwhl_t *);
static inline bitfld_t peek(const dwhl_t *, size_t);
static inline bitfld_t last_fld(const dwhl_t *);
static inline dwhl_t *max_sz(const dwhl_t *, const dwhl_t *);
static inline dwhl_t *min_sz(const dwhl_t *, const dwhl_t *);
static inline dwhl_t *set_bit(dwhl_t *, shift_t, bool);

// Performs division, stores result in tar
dwhl_t *do_div(dwhl_t *tar, const dwhl_t *val, bool rem) {
    if (!tar || !val) {
        errno = EINVAL;
        return NULL;
    }
    assert_lval(tar);

    dwhl_t *tmp;
    const bool val_rval = is_rval(val);

    if (!dwhl_cmp(val, dwhl_zero)) {
        clr_rval(val, val_rval);
        errno = EDOM;
        return NULL;
    }
    if (!dwhl_cmp(tar, dwhl_zero)) {
        clr_rval(val, val_rval);
        return tar;
    }
    switch (dwhl_cmp(tar, val)) {
    case -1:
        tmp = dwhl_eq(tar, rem ? val : dwhl_zero);
        clr_rval(val, val_rval);
        return tmp;
    case  0:
        tmp = dwhl_eq(tar, rem ? dwhl_zero : dwhl_one);
        clr_rval(val, val_rval);
        return tmp;
    }

    dwhl_t *tar_abs = dwhl_abs(tar), *val_abs;

    if (!tar_abs) {
        clr_rval(val, val_rval);
        return NULL;
    }
    val_abs = dwhl_abs(val);
    if (!val_abs) {
        clr_rval(val, val_rval);
        clr_rval(tar_abs, true);
        return NULL;
    }
    tar_abs->rval = false;
    val_abs->rval = false;

    shift_t cur = sig_bits(tar_abs) - sig_bits(val_abs);

    dwhl_swp(tar, dwhl_rshifteq(tar_abs, cur));
    dwhl_andeq(tar_abs, dwhl_slshift(dwhl_zero, cur));
    if (errno) {
        clr_rval(val, val_rval);
        clr_rval(tar_abs, true);
        clr_rval(val_abs, true);
        return NULL;
    }
    for (;;) {
        dwhl_subeq(tar, dwhl_muleq(set_bit(tar_abs, cur + 1,
            dwhl_cmp(tar, val_abs) < 0 ? 0 : 1   // Primitive division
        ), val_abs));
        if (!cur)
            break;
        dwhl_lshifteq(tar, 1);
        tar->bits[0] |= get_bit(tar_abs, --cur);
    }
    if (errno)
        tmp = NULL;
    else if (rem)
        tmp = dwhl_isneg(val) ? dwhl_negeq(tar) : tar;
    else {
        if (dwhl_isneg(tar) ^ dwhl_isneg(val))
            dwhl_negeq(tar_abs);
        tmp = dwhl_swp(tar, tar_abs);
    }
    clr_rval(val, val_rval);
    clr_rval(tar_abs, true);
    clr_rval(val_abs, true);
    return tmp;
}

// Performs left shift, stores result in tar
dwhl_t *do_lshift(dwhl_t *tar, shift_t shift, bitfld_t fill) {
    if (!tar) {
        errno = EINVAL;
        return NULL;
    }
    assert_lval(tar);

    const shdiv_t result = sh_div(shift, BITFLD_BITS);
    const shift_t move = result.quot, pad = padding(tar);

    if (shift > pad) {
        shift_t add = (shift - pad) / BITFLD_BITS;

        add += ((shift - pad) % BITFLD_BITS) != 0;
        if (tar->size > BITFLD_CT_MAX - add - 1) {   // Result too large
            errno = ERANGE;
            return NULL;
        }
        extend(tar, tar->size + add);
        if (errno)
            return NULL;
    }

    bitfld_t carry = 0, tmp;

    shift = result.rem;
    memmove(tar->bits + move, tar->bits, (tar->size - move) * sizeof(bitfld_t));
    memset(tar->bits, (int) fill, move * sizeof(bitfld_t));
    for (size_t i = move, lim = tar->size; i < lim; ++i) {
        // Get carry
        tmp = (tar->bits[i] & ~(BITFLD_MAX >> shift)) >> BITFLD_BITS - shift;

        // Apply carry
        tar->bits[i] = (tar->bits[i] << shift) | carry;

        if (i == move)
            tar->bits[i] |= fill >> (sizeof(bitfld_t) * 8 - shift);
        carry = tmp;
    }
    return tar;
}

// Recursive multiplication, stores result in max
dwhl_t *do_mul(dwhl_t *max, dwhl_t *min) {
    dwhl_t first;
    const shift_t sig = sig_bits(min) - 1;

    dwhl_initi(&first, max);
    dwhl_lshifteq(max, sig);
    dwhl_andeq(min, dwhl_slshift(dwhl_zero, sig));
    if (errno) {
        dwhl_clr(&first);
        return NULL;
    }
    if      (!dwhl_cmp(min, dwhl_one))  dwhl_addeq(max, &first);
    else if (dwhl_cmp(min, dwhl_zero))  dwhl_addeq(max, do_mul(&first, min));
    dwhl_clr(&first);
    return errno ? NULL : max;
}

// Extend integer to specified size
dwhl_t *extend(dwhl_t *tar, size_t resize) {
    if (resize > BITFLD_CT_MAX) {   // Integer too large
        errno = ERANGE;
        return NULL;
    }
    tar->bits = realloc(tar->bits, resize * sizeof(bitfld_t));
    if (errno)
        return NULL;
    memset(tar->bits + tar->size, ~0 * dwhl_isneg(tar), resize - tar->size);
    tar->size = resize;
    return tar;
}

// Returns positive integer with most significant bits
dwhl_t *max_sig(const dwhl_t *lhs, const dwhl_t *rhs) {
    const dwhl_t *max = max_sz(lhs, rhs), *min = ptr_rem(lhs, rhs, max);
    size_t i;

    for (i = max->size - 1; i >= min->size; --i) {
        if (max->bits[i])
            return (dwhl_t *) (max == lhs ? lhs : rhs);
    }
    while (i) {
        if (max->bits[i] > min->bits[i])
            return (dwhl_t *) (max == lhs ? lhs : rhs);
        if (max->bits[--i] < min->bits[i])
            return (dwhl_t *) (min == lhs ? lhs : rhs);
        --i;
    }
    return (dwhl_t *) lhs;
}

// Returns padding between most significant 1 bit and end of bit buffer, in bits
shift_t padding(const dwhl_t *val) {
    for (size_t i = 0, j = val->size - 1; i < val->size; ++i, --j) {
        if (val->bits[j])
            return (i + 1) * BITFLD_BITS - bitfld_sig(val->bits[j]);
    }
    return val->size * BITFLD_BITS; // Integer equals 0
}

// Returns number of significant bits in positive integer
shift_t sig_bits(const dwhl_t *val) {
    bitfld_t cur;

    for (size_t i = val->size - 1;; --i) {
        if ((cur = val->bits[i]))
            return i * sizeof(bitfld_t) + bitfld_sig(cur);
        if (!i)
            break;
    }
    return 0;
}

// If temporary, free integer
void clr_rval(const dwhl_t *val, bool tmp) {
    if (tmp) {
        free(val->bits);
        free((dwhl_t *) val);
    }
    return;
}

// Returns state of bit in integer
bool get_bit(const dwhl_t *val, shift_t index) {
    const shdiv_t result = sh_div(index, sizeof(bitfld_t));

    return val->bits[result.quot] & 1 << result.rem;
}

// If temporary, returns true and sets .rval to false
bool is_rval(const dwhl_t *val) {
    bool tmp;

    if (tmp = val->rval)
        ((dwhl_t *) val)->rval = false;
    return tmp;
}

// Returns value representing insignificant bits in an integer
bitfld_t insig_val(const dwhl_t *val) {
    return BITFLD_MAX * dwhl_isneg(val);
}

/* Returns bitfield at position in bit buffer
 * If position exceeds size of buffer, returns insignificant value */
bitfld_t peek(const dwhl_t *val, size_t at) {
    if (at >= val->size)
        return insig_val(val);
    return val->bits[at];
}

// Returns final bitfield in bit buffer
bitfld_t last_fld(const dwhl_t *val) {
    return val->bits[val->size - 1];
}

// Returns integer of largest bit buffer
dwhl_t *max_sz(const dwhl_t *lhs, const dwhl_t *rhs) {
    return (dwhl_t *) (lhs->size > rhs->size ? lhs : rhs);
}

// Returns integer of smallest bitfield size
dwhl_t *min_sz(const dwhl_t *lhs, const dwhl_t *rhs) {
    return (dwhl_t *) (lhs->size < rhs->size ? rhs : lhs);
}

// Sets bit in integer to specified state
dwhl_t *set_bit(dwhl_t *tar, shift_t index, bool state) {
    const shdiv_t result = sh_div(index, sizeof(bitfld_t));

    tar->bits[result.quot] ^= 1 << result.rem;
    return tar;
}

// ---- Basic Utilities ----

export integr_t dwhl_casts(const dwhl_t *val, size_t size) {
    if (!val) {
        errno = EINVAL;
        return 0;
    }
    if (sig_bits(val) >= size * 8) {
        errno = EOVERFLOW;
        clr_rval(val, val->rval);
        return 0;
    }

    integr_t tmp = *(integr_t *) (val->bits + INTEGR_OFF);

    clr_rval(val, val->rval);
    return tmp;
}
export uintegr_t dwhl_castu(const dwhl_t *val, size_t size) {
    if (!val) {
        errno = EINVAL;
        return 0;
    }
    if (val && val->bits[val->size - 1] & SIGN_BIT || sig_bits(val) > size * 8) {
        errno = EOVERFLOW;
        clr_rval(val, val->rval);
        return 0;
    }

    uintegr_t tmp = *(uintegr_t *) (val->bits + INTEGR_OFF);

    clr_rval(val, val->rval);
    return tmp;
}
export int dwhl_cmp(const dwhl_t *lhs, const dwhl_t *rhs) {
    if (!lhs) {
        if (rhs)
            clr_rval(rhs, rhs->rval);
        errno = EINVAL;
        return 2;
    }
    if (!rhs) {
        clr_rval(lhs, lhs->rval);
        errno = EINVAL;
        return 2;    
    }

    const bool lhs_rval = is_rval(lhs), rhs_rval = is_rval(rhs), lhs_sign = dwhl_isneg(lhs);

    if (lhs_sign ^ dwhl_isneg(rhs)) {
        clr_rval(lhs, lhs_rval);
        clr_rval(rhs, rhs_rval);
        return lhs_sign ? -1 : 1;
    }

    int cmpval;
    const size_t final = max_sz(lhs, rhs)->size - 1;

    for (size_t i = final;; --i) {
        cmpval = (peek(lhs, i) > peek(rhs, i)) - (lhs->bits[i] < rhs->bits[i]);
        if (cmpval) {
            clr_rval(lhs, lhs_rval);
            clr_rval(rhs, rhs_rval);
            return lhs_sign ? -cmpval : cmpval;
        }
        if (!i) {
            clr_rval(lhs, lhs_rval);
            clr_rval(rhs, rhs_rval);
            return 0;
        }
    }
}
export dwhl_t *dwhl_eq(dwhl_t *restrict tar, const dwhl_t *restrict val) {
    if (!val) {
        errno = EINVAL;
        return NULL;
    }
    if (!tar) {
        clr_rval(val, val->rval);
        errno = EINVAL;
        return NULL;
    }
    assert_lval(tar);

    dwhl_t *tmp;

    if (val->rval) {
        ((dwhl_t *) val)->rval = false;
        tmp = dwhl_swp(tar, (dwhl_t *) val);
        clr_rval(val, true);
        return tmp;
    }
    if (tar->size < val->size) {
        free(tar->bits);
        tmp = dwhl_initi(tar, val);
        return tmp;
    }
    memcpy(tar->bits, val->bits, val->size * sizeof(bitfld_t));
    memset(tar->bits + val->size, ~0 * dwhl_isneg(val), (tar->size - val->size) * sizeof(bitfld_t));
    return tar;
}
/* export dwhl_t *dwhl_eqf(dwhl_t *restrict tar, const ddec_t *restrict val) {
    if (!val) {
        errno = EINVAL;
        return NULL;
    }
    if (!tar) {
        clr_rval(val, val->rval);
        errno = EINVAL;
        return NULL;
    }
    assert_lval(tar);

    // ...

} */
export dwhl_t *dwhl_eqs(dwhl_t *restrict tar, integr_t val) {
    if (!tar) {
        errno = EINVAL;
        return NULL;
    }
    assert_lval(tar);
    memcpy(tar->bits, &val, sizeof(bitfld_t));
    memset(tar->bits + 1, ~0 * (val < 0), (tar->size - 1) * sizeof(bitfld_t));
    return tar;
}
export dwhl_t *dwhl_equ(dwhl_t *restrict tar, uintegr_t val) {
    if (!tar) {
        errno = EINVAL;
        return NULL;
    }
    assert_lval(tar);
    memcpy(tar->bits, &val, sizeof(bitfld_t));
    memset(tar->bits + 1, 0, (tar->size - 1) * sizeof(bitfld_t));
    return tar;
}
/* export dwhl_t *dwhl_initf(dwhl_t *restrict tar, const ddec_t *val) {
    if (!val) {
        errno = EINVAL;
        return NULL;
    }
    if (!tar) {
        clr_rval(val, val->rval);
        errno = EINVAL;
        return NULL;
    }
    assert_lval(tar);

    // ...

    clr_rval(val, val->rval);
    tar->rval = false;
    return tar;
} */
export dwhl_t *dwhl_initi(dwhl_t *restrict tar, const dwhl_t *val) {
    if (!val) {
        errno = EINVAL;
        return NULL;
    }
    if (!tar) {
        clr_rval(val, val->rval);
        errno = EINVAL;
        return NULL;
    }
    tar->bits = malloc((tar->size = val->size) * sizeof(bitfld_t));
    if (!tar->bits)
        return NULL;
    memcpy(tar->bits, val->bits, val->size * sizeof(bitfld_t));
    clr_rval(val, val->rval);
    tar->rval = false;
    return tar;
}
export dwhl_t *dwhl_inits(dwhl_t *restrict tar, integr_t val) {
    if (!tar) {
        errno = EINVAL;
        return NULL;
    }
    tar->bits = malloc((tar->size = 2) * sizeof(bitfld_t));
    if (!tar->bits)
        return NULL;
    tar->bits[1] = BITFLD_MAX * !!(val & SIGN_BIT);
    tar->bits[0] = val;
    tar->rval = false;
    return tar;
}
export dwhl_t *dwhl_initu(dwhl_t *restrict tar, uintegr_t val) {
    if (!tar) {
        errno = EINVAL;
        return NULL;
    }
    tar->bits = malloc((tar->size = 2) * sizeof(bitfld_t));
    if (!tar->bits)
        return NULL;
    tar->bits[1] = 0;
    tar->bits[0] = val;
    tar->rval = false;
    return tar;
}
export bool dwhl_isneg(const dwhl_t *restrict val) {
    if (!val) {
        errno = EINVAL;
        return 0;
    }

    const bool tmp = val->bits[val->size - 1] & SIGN_BIT;

    clr_rval(val, val->rval);
    return tmp;
}
export dwhl_t *dwhl_swp(dwhl_t *restrict ret, dwhl_t *restrict val) {
    if (!val) {
        errno = EINVAL;
        return NULL;
    }
    if (!ret) {
        errno = EINVAL;
        return NULL;
    }
    assert_lval(ret);
    assert_lval(val);

    dwhl_t tmp = *ret;

    *ret = *val;
    *val = tmp;
    return ret;
}

// ---- Basic Arthmetic ----

export dwhl_t *dwhl_abseq(dwhl_t *tar) {
    assert_lval(tar);
    return dwhl_isneg(tar) ? dwhl_negeq(tar) : tar;
}
export dwhl_t *dwhl_addeq(dwhl_t *tar, const dwhl_t *val) {
    if (!val) {
        errno = EINVAL;
        return NULL;
    }
    if (!tar) {
        clr_rval(val, val->rval);
        errno = EINVAL;
        return NULL;
    }
    assert_lval(tar);

    const bool val_rval = is_rval(val);
    dwhl_t *tar_abs = dwhl_abs(tar), *val_abs;

    if (!tar_abs) {
        clr_rval(val, val_rval);
        return NULL;
    }
    val_abs = dwhl_abs(val);
    if (!val_abs) {
        clr_rval(tar_abs, true);
        clr_rval(val, val_rval);
        return NULL;
    }

    const dwhl_t *max = max_sig(tar_abs, val_abs);
    bool tmp = last_fld(max) != insig_val(max);

    // Append bit buffer if needed, with room for overflow
    if (tar->size < max->size + tmp && !extend(tar, max->size + tmp)) {
        clr_rval(tar_abs, true);
        clr_rval(val_abs, true);
        clr_rval(val, val_rval);
        return NULL;
    } 

    bool overflow = false;

    for (size_t i = 0, lim = tar->size; i < lim; ++i) {
        tmp = tar->bits[i] > BITFLD_MAX - val->bits[i] || val->bits[i] > BITFLD_MAX - tar->bits[i];
        tar->bits[i] += val->bits[i] + overflow;
        overflow = tmp;
    }
    clr_rval(tar_abs, true);
    clr_rval(val_abs, true);
    clr_rval(val, val_rval);
    return tar;
}
export dwhl_t *dwhl_andeq(dwhl_t *tar, const dwhl_t *val) {
    if (!val) {
        errno = EINVAL;
        return NULL;
    }
    if (!tar) {
        clr_rval(val, val->rval);
        errno = EINVAL;
        return NULL;
    }
    assert_lval(tar);
    for (size_t i = 0, lim = min_sz(tar, val)->size; i < lim; ++i)
        tar->bits[i] &= val->bits[i];
    clr_rval(val, val->rval);
    return tar;
}
export dwhl_t *dwhl_negeq(dwhl_t *tar) {
    assert_lval(tar);
    return dwhl_addeq(dwhl_noteq(tar), dwhl_one);
}
export dwhl_t *dwhl_noteq(dwhl_t *tar) {
    if (!tar) {
        errno = EINVAL;
        return NULL;
    }
    assert_lval(tar);
    for (size_t i = 0, lim = tar->size; i < lim; ++i)
        tar->bits[i] = ~tar->bits[i];
    return tar;
}
export dwhl_t *dwhl_oreq(dwhl_t *tar, const dwhl_t *val) {
    if (!val) {
        errno = EINVAL;
        return NULL;
    }
    if (!tar) {
        clr_rval(val, val->rval);
        errno = EINVAL;
        return NULL;
    }
    assert_lval(tar);
    if (tar->size < val->size) {
        extend(tar, val->size);
        if (errno) {
            clr_rval(val, val->rval);
            return NULL;
        }
    } 
    for (size_t i = 0, lim = tar->size; i < lim; ++i)
        tar->bits[i] |= val->bits[i];
    clr_rval(val, val->rval);
    return tar;
}
export dwhl_t *dwhl_subeq(dwhl_t *tar, const dwhl_t *val) {
    return dwhl_addeq(tar, dwhl_neg(val));
}
export dwhl_t *dwhl_xoreq(dwhl_t *tar, const dwhl_t *val) {
    if (!val) {
        errno = EINVAL;
        return NULL;
    }
    if (!tar) {
        clr_rval(val, val->rval);
        errno = EINVAL;
        return NULL;
    }
    assert_lval(tar);
    if (tar->size < val->size) {
        extend(tar, val->size);
        if (errno) {
            clr_rval(val, val->rval);
            return NULL;
        }
    }
    for (size_t i = 0, lim = tar->size; i < lim; ++i)
        tar->bits[i] ^= val->bits[i];
    clr_rval(val, val->rval);
    return tar;
}
export dwhl_t *dwhl_diveq(dwhl_t *tar, const dwhl_t *val) {
    return do_div(tar, val, false);
}
export dwhl_t *dwhl_modeq(dwhl_t *tar, const dwhl_t *val) {
    return do_div(tar, val, true);
}
export dwhl_t *dwhl_muleq(dwhl_t *tar, const dwhl_t *val) {
    if (!val) {
        errno = EINVAL;
        return NULL;
    }
    if (!tar) {
        clr_rval(val, val->rval);
        errno = EINVAL;
        return NULL;
    }
    assert_lval(tar);

    const bool val_rval = is_rval(val);

    if (!dwhl_cmp(tar, dwhl_zero) || !dwhl_cmp(val, dwhl_zero)) {
        clr_rval(val, val_rval);
        return dwhl_eq(tar, dwhl_zero);
    }

    dwhl_t *tar_abs = dwhl_abs(tar), *val_abs;

    if (!tar_abs) {
        clr_rval(val, val_rval);
        return NULL;
    }
    val_abs = dwhl_abs(val);
    if (!val_abs) {
        clr_rval(tar_abs, true);
        clr_rval(val, val_rval);
        return NULL;
    }
    
    dwhl_t *max = max_sig(tar_abs, val_abs), *min = ptr_rem(tar_abs, val_abs, max);

    if (dwhl_cmp(min, maxmul) > 0) {    // Shift overflow
        clr_rval(tar_abs, true);
        clr_rval(val_abs, true);
        clr_rval(val, val_rval);
        errno = ERANGE;
        return NULL;
    }

    const bool negate = dwhl_isneg(tar) ^ dwhl_isneg(val);

    dwhl_swp(tar, do_mul(max, min));
    clr_rval(tar_abs, true);
    clr_rval(val_abs, true);
    clr_rval(val, val_rval);
    if (errno)
        return NULL;
    return negate ? dwhl_negeq(tar) : tar;   
}
export dwhl_t *dwhl_lshifteq(dwhl_t *tar, shift_t shift) {
    return do_lshift(tar, shift, 0);
}
export dwhl_t *dwhl_slshifteq(dwhl_t *tar, shift_t shift) {
    return do_lshift(tar, shift, BITFLD_MAX);
}
export dwhl_t *dwhl_rshifteq(dwhl_t *tar, shift_t shift) {
    if (!tar) {
        errno = EINVAL;
        return NULL;
    }
    assert_lval(tar);

    const shdiv_t result = sh_div(shift, BITFLD_BITS);
    const shift_t move = result.quot;

    if (move >= tar->size)
        return dwhl_eq(tar, dwhl_zero);

    bitfld_t carry = 0, tmp;

    shift = result.rem;
    memmove(tar->bits, tar->bits + move, (tar->size - move) * sizeof(bitfld_t));
    memset(tar->bits + (tar->size - move), 0, move * sizeof(bitfld_t));
    for (size_t i = tar->size - move - 1, j = tar->size - 1;; --i, --j) {
        // Get carry
        tmp = (tar->bits[j] & ~(BITFLD_MAX << shift)) << BITFLD_BITS - shift ; 

        // Apply carry  
        tar->bits[j] = (tar->bits[j] >> shift) | carry;

        carry = tmp;
        if (!i)
            return tar;
    }
}

export dwhl_t *dwhl_abs(const dwhl_t *val) { BUILD_UNARY(abs, val); }
export dwhl_t *dwhl_neg(const dwhl_t *val) { BUILD_UNARY(neg, val); }
export dwhl_t *dwhl_not(const dwhl_t *val) { BUILD_UNARY(not, val); }

export dwhl_t *dwhl_sub(const dwhl_t *lhs, const dwhl_t *rhs) { BUILD_BINARY(sub, lhs, rhs); }
export dwhl_t *dwhl_div(const dwhl_t *lhs, const dwhl_t *rhs) { BUILD_BINARY(div, lhs, rhs); }
export dwhl_t *dwhl_mod(const dwhl_t *lhs, const dwhl_t *rhs) { BUILD_BINARY(mod, lhs, rhs); }

export dwhl_t *dwhl_add(const dwhl_t *lhs, const dwhl_t *rhs) { BUILD_COMMUT(add, lhs, rhs); }
export dwhl_t *dwhl_and(const dwhl_t *lhs, const dwhl_t *rhs) { BUILD_COMMUT(and, lhs, rhs); }
export dwhl_t *dwhl_or(const dwhl_t *lhs, const dwhl_t *rhs)  { BUILD_COMMUT(or, lhs, rhs);  }
export dwhl_t *dwhl_xor(const dwhl_t *lhs, const dwhl_t *rhs) { BUILD_COMMUT(xor, lhs, rhs); }
export dwhl_t *dwhl_mul(const dwhl_t *lhs, const dwhl_t *rhs) { BUILD_COMMUT(mul, lhs, rhs); }

export dwhl_t *dwhl_lshift(const dwhl_t *val, shift_t shift)  { BUILD_SHIFT(lshift, val, shift);  }
export dwhl_t *dwhl_slshift(const dwhl_t *val, shift_t shift) { BUILD_SHIFT(slshift, val, shift); }
export dwhl_t *dwhl_rshift(const dwhl_t *val, shift_t shift)  { BUILD_SHIFT(rshift, val, shift);  }
