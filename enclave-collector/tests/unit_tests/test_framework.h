#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Simple test framework
typedef struct {
    int tests_run;
    int tests_passed;
    int tests_failed;
} test_results_t;

static test_results_t g_test_results = {0, 0, 0};

#define TEST_ASSERT(condition, message) \
    do { \
        g_test_results.tests_run++; \
        if (condition) { \
            g_test_results.tests_passed++; \
            printf("  ✓ %s\n", message); \
        } else { \
            g_test_results.tests_failed++; \
            printf("  ✗ %s (line %d)\n", message, __LINE__); \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL(expected, actual, message) \
    TEST_ASSERT((expected) == (actual), message)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT((ptr) != NULL, message)

#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT((ptr) == NULL, message)

#define TEST_ASSERT_STRING_EQUAL(expected, actual, message) \
    TEST_ASSERT(strcmp((expected), (actual)) == 0, message)

#define RUN_TEST(test_func) \
    do { \
        printf("\nRunning %s...\n", #test_func); \
        test_func(); \
    } while(0)

static void print_test_summary(void) __attribute__((unused));
static void print_test_summary(void) {
    printf("\n=== Test Summary ===\n");
    printf("Tests run: %d\n", g_test_results.tests_run);
    printf("Tests passed: %d\n", g_test_results.tests_passed);
    printf("Tests failed: %d\n", g_test_results.tests_failed);
    
    if (g_test_results.tests_failed == 0) {
        printf("All tests passed! ✓\n");
    } else {
        printf("Some tests failed! ✗\n");
    }
}

static int get_test_result(void) __attribute__((unused));
static int get_test_result(void) {
    return g_test_results.tests_failed == 0 ? 0 : 1;
}

#endif // TEST_FRAMEWORK_H
