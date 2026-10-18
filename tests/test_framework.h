/**
 * Simple Test Framework for lgx_arena.h
 * 
 * Minimal testing framework with no dependencies
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test statistics
static struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    const char* current_test;
} g_test_stats = {0, 0, 0, NULL};

// Color codes for terminal output
#define COLOR_GREEN "\033[0;32m"
#define COLOR_RED "\033[0;31m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_RESET "\033[0m"

// Test macros
#define TEST_BEGIN(name) \
    do { \
        g_test_stats.current_test = name; \
        g_test_stats.total_tests++; \
        printf("  [ RUN  ] %s\n", name); \
    } while(0)

#define TEST_END() \
    do { \
        g_test_stats.passed_tests++; \
        printf("  [" COLOR_GREEN "  OK  " COLOR_RESET "] %s\n", g_test_stats.current_test); \
    } while(0)

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("  [" COLOR_RED " FAIL " COLOR_RESET "] %s\n", g_test_stats.current_test); \
            printf("    Assertion failed: %s\n", message); \
            printf("    at %s:%d\n", __FILE__, __LINE__); \
            g_test_stats.failed_tests++; \
            g_test_stats.total_tests--; \
            g_test_stats.passed_tests--; \
            return 1; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(a, b, message) \
    do { \
        if ((a) != (b)) { \
            printf("  [" COLOR_RED " FAIL " COLOR_RESET "] %s\n", g_test_stats.current_test); \
            printf("    Assertion failed: %s\n", message); \
            printf("    Expected: %ld, Got: %ld\n", (long)(b), (long)(a)); \
            printf("    at %s:%d\n", __FILE__, __LINE__); \
            g_test_stats.failed_tests++; \
            g_test_stats.total_tests--; \
            g_test_stats.passed_tests--; \
            return 1; \
        } \
    } while(0)

#define TEST_ASSERT_NE(a, b, message) \
    do { \
        if ((a) == (b)) { \
            printf("  [" COLOR_RED " FAIL " COLOR_RESET "] %s\n", g_test_stats.current_test); \
            printf("    Assertion failed: %s\n", message); \
            printf("    Expected not equal: %ld\n", (long)(b)); \
            printf("    at %s:%d\n", __FILE__, __LINE__); \
            g_test_stats.failed_tests++; \
            g_test_stats.total_tests--; \
            g_test_stats.passed_tests--; \
            return 1; \
        } \
    } while(0)

#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT((ptr) == NULL, message)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT((ptr) != NULL, message)

// Test suite macros
#define TEST_SUITE_BEGIN(name) \
    int main(void) { \
        printf("\n" COLOR_YELLOW "=== Test Suite: %s ===" COLOR_RESET "\n\n", name);

#define TEST_SUITE_END() \
        printf("\n" COLOR_YELLOW "=== Test Results ===" COLOR_RESET "\n"); \
        printf("Total:  %d\n", g_test_stats.total_tests); \
        printf("Passed: " COLOR_GREEN "%d" COLOR_RESET "\n", g_test_stats.passed_tests); \
        printf("Failed: " COLOR_RED "%d" COLOR_RESET "\n", g_test_stats.failed_tests); \
        if (g_test_stats.failed_tests == 0) { \
            printf("\n" COLOR_GREEN "All tests passed!" COLOR_RESET "\n\n"); \
            return 0; \
        } else { \
            printf("\n" COLOR_RED "Some tests failed!" COLOR_RESET "\n\n"); \
            return 1; \
        } \
    }

// Test runner macro
#define RUN_TEST(test_func) \
    do { \
        if (test_func() != 0) { \
            /* Test failed, already counted */ \
        } \
    } while(0)

#endif // TEST_FRAMEWORK_H
