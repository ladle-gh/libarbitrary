#include <ladle/common/lib.h>
#include <ladle/common/ptrcmp.h>

#include "etc.h"
#include "arbitrary.h"

#define PREFIX  ddec

// ---- Constants ----

// Convenience constants
export const ddec_t *const ddec_one  = &(ddec_t) {/* TODO */};
export const ddec_t *const ddec_zero = &(ddec_t) {/* TODO */};

// Maximum value to be multiplied by
static ddec_t *ddec_maxmul = &(ddec_t) {/* TODO */};

// ---- Helper Functions ----
