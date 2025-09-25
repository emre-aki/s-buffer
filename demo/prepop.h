/*
 *  prepop.h
 *  s-buffer
 *
 *  Created by Emre Akı on 2025-09-12.
 *
 *  SYNOPSIS:
 *      Predefined lists of spans to prepopulate an S-Buffer with to test out
 *      various scenarios
 */

#ifndef prepop_h

#include "demodef.h"

#define prepop_h
#define prepop_h_TEST_CASES TEST_CASES

#define N_CASES 15

typedef struct {
    const seg2_t* segs;
    const size_t  segs_count;
} test_case_t;

extern const test_case_t TEST_CASES[N_CASES];

#endif
