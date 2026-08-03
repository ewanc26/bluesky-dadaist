/**
 * test.h — a deliberately tiny test harness (mirrors Wolfram's).
 * No framework dependency: assert-and-report macros, a shared pass/fail
 * counter, and a summary printed by TEST_SUMMARY.
 */

#ifndef DADA_TEST_H
#define DADA_TEST_H

#include <stdio.h>

static int dada_test_pass = 0;
static int dada_test_fail = 0;

#define TEST_CHECK(cond)                                                      \
    do {                                                                      \
        if (cond) {                                                           \
            dada_test_pass++;                                                 \
        } else {                                                              \
            dada_test_fail++;                                                 \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
        }                                                                     \
    } while (0)

#define TEST_SUMMARY()                                                        \
    do {                                                                      \
        printf("%d passed, %d failed\n", dada_test_pass, dada_test_fail);     \
        return dada_test_fail == 0 ? 0 : 1;                                   \
    } while (0)

#endif /* DADA_TEST_H */
