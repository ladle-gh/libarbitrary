#ifndef LADLE_ARBITRARY_GLOBAL_H
#define LADLE_ARBITRARY_GLOBAL_H
#include <stdbool.h>

#include <ladle/common/header.h>
#include <ladle/common/ptrcmp.h>

#include "arbitrary.h"

// ---- Procedure Shorthands ----

// Function declaration of unary operation returning rvalue

#define   BUILD_UNARY(id, val)          _BUILD_UNARY(PREFIX, id, val)
#define  _BUILD_UNARY(prefix, id, val) __BUILD_UNARY(prefix, id, val)
#define __BUILD_UNARY(prefix, id, val) {             \
    if (!(val)->rval)                               \
        (val) = prefix##_tmp(val);                  \
    prefix##_t *tmp;                                \
    ((prefix##_t *) (val))->rval = false;           \
    tmp = prefix##_##id##eq((prefix##_t *) (val));  \
    ((prefix##_t *) (val))->rval = true;            \
    return tmp;                                     \
}

// Function declaration of binary, commutative operation returning rvalue
#define   BUILD_COMMUT(id, lhs, rhs)          _BUILD_COMMUT(PREFIX, id, lhs, rhs)
#define  _BUILD_COMMUT(prefix, id, lhs, rhs) __BUILD_COMMUT(prefix, id, lhs, rhs)
#define __BUILD_COMMUT(prefix, id, lhs, rhs) {                  \
    prefix##_t *tmp;                                            \
    if ((lhs)->rval) {                                          \
        ((prefix##_t *) (lhs))->rval = false;                   \
        tmp = prefix##_##id##eq((prefix##_t *) (lhs), (rhs));   \
        ((prefix##_t *) (lhs))->rval = true;                    \
        return tmp;                                             \
    } else if ((rhs)->rval) {                                   \
        ((prefix##_t *) (rhs))->rval = false;                   \
        tmp = prefix##_##id##eq((prefix##_t *) (rhs), (lhs));   \
        ((prefix##_t *) (rhs))->rval = true;                    \
        return tmp;                                             \
    }                                                           \
    prefix##_t *cpy = prefix##_tmp((lhs));                      \
    ((prefix##_t *) cpy)->rval = false;                         \
    tmp = prefix##_##id##eq((prefix##_t *) cpy, (rhs));         \
    ((prefix##_t *) cpy)->rval = true;                          \
    return tmp;                                                 \
}

// Function declaration of binary, noncommutative operation returning rvalue
#define   BUILD_BINARY(id, lhs, rhs)          _BUILD_BINARY(PREFIX, id, lhs, rhs)
#define  _BUILD_BINARY(prefix, id, lhs, rhs) __BUILD_BINARY(prefix, id, lhs, rhs)
#define __BUILD_BINARY(prefix, id, lhs, rhs) {                  \
    prefix##_t *tmp;                                            \
    if ((lhs)->rval) {                                          \
        ((prefix##_t *) (lhs))->rval = false;                   \
        tmp = prefix##_##id##eq((prefix##_t *) (lhs), (rhs));   \
        ((prefix##_t *) (lhs))->rval = true;                    \
        return tmp;                                             \
    }                                                           \
    prefix##_t *cpy = prefix##_tmp((lhs));                      \
    ((prefix##_t *) cpy)->rval = false;                         \
    tmp = prefix##_##id##eq((prefix##_t *) cpy, (rhs));         \
    ((prefix##_t *) cpy)->rval = true;                          \
    return tmp;                                                 \
}

// Function declaration of bitwise shift returning rvalue
#define   BUILD_SHIFT(id, val, shift)          _BUILD_SHIFT(PREFIX, id, val, shift)
#define  _BUILD_SHIFT(prefix, id, val, shift) __BUILD_SHIFT(prefix, id, val, shift)
#define __BUILD_SHIFT(prefix, id, val, shift) {             \
    if (!(val)->rval)                                       \
        (val) = prefix##_tmp(val);                          \
    prefix##_t *tmp;                                        \
    ((prefix##_t *) (val))->rval = false;                   \
    tmp = prefix##_##id##eq((prefix##_t *) (val), shift);   \
    ((prefix##_t *) (val))->rval = true;                    \
    return tmp;                                             \
}

// ---- Constants ----

// Offset from pointer to bit buffer whose address returns an integral value
#define INTEGR_OFF  (sizeof(integr_t) / sizeof(bitfld_t) - 1)

// ---- Helper Functions ----

/* Asserts value is lvalue
 * If assertion fails, terminates program */
#define  assert_lval(tar) _assert_lval(prefix##_t, tar)
#define _assert_lval(type, tar) {                   \
    if ((tar)->rval) {                              \
        printf(                                     \
            "arbitrary.h: expression must be a "    \
            "modifiable lvalue: ("#type" *) %p\n"   \
        , tar);                                     \
        exit(EXIT_FAILURE);                         \
    }                                               \
}

// Defined, safe addition of x and y, subtracted by difference
static inline void *ptr_rem(const void *(lhs), const void *(rhs), const void *diff) {
    return (void *) (((ptr_cast((lhs)) >> 1) + (ptr_cast((rhs)) >> 1) - (ptr_cast(diff) >> 1)) << 1);
}

// Returns # of significant bits in bitfield
static inline unsigned char bitfld_sig(bitfld_t bits) {
    unsigned char ct = 0;

    while (bits) {
        bits >>= 1;
        ++ct;
    }
    return ct;
}

#include <ladle/common/end_header.h>
#endif  // #ifndef LADLE_ARBITRARY_GLOBAL_H