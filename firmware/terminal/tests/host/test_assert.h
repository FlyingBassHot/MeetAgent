/*
 * Tiny host-test assertion helpers. Tests continue after a failure so one run
 * reports every broken check, and main() exits non-zero if any failed.
 */
#pragma once

#include <stdio.h>
#include <string.h>

extern int g_test_checks;
extern int g_test_failures;

#define RUN(fn)                                                                \
    do {                                                                       \
        printf("[ RUN ] %s\n", #fn);                                           \
        fn();                                                                  \
    } while (0)

#define TEST_ASSERT_TRUE(cond)                                                 \
    do {                                                                       \
        ++g_test_checks;                                                       \
        if (!(cond)) {                                                         \
            ++g_test_failures;                                                 \
            printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_FALSE(cond) TEST_ASSERT_TRUE(!(cond))

#define TEST_ASSERT_EQ_INT(expected, actual)                                   \
    do {                                                                       \
        ++g_test_checks;                                                       \
        long long exp_ = (long long)(expected);                                \
        long long act_ = (long long)(actual);                                  \
        if (exp_ != act_) {                                                    \
            ++g_test_failures;                                                 \
            printf("[FAIL] %s:%d: expected %lld, got %lld (%s)\n", __FILE__,  \
                   __LINE__, exp_, act_, #actual);                             \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_STR_EQ(expected, actual)                                   \
    do {                                                                       \
        ++g_test_checks;                                                       \
        const char *exp_ = (expected);                                         \
        const char *act_ = (actual);                                           \
        if (exp_ == NULL || act_ == NULL || strcmp(exp_, act_) != 0) {         \
            ++g_test_failures;                                                 \
            printf("[FAIL] %s:%d: expected \"%s\", got \"%s\"\n", __FILE__,    \
                   __LINE__, exp_ ? exp_ : "(null)", act_ ? act_ : "(null)");  \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_NULL(ptr) TEST_ASSERT_TRUE((ptr) == NULL)
#define TEST_ASSERT_NOT_NULL(ptr) TEST_ASSERT_TRUE((ptr) != NULL)
