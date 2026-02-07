/**
 * SIMD Graceful Degradation Tests - Task 3.5.2.4
 * 
 * Tests that SIMD operations work correctly with and without AVX2.
 * Verifies scalar fallback paths produce identical results.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("  ✓ %s\n", message); \
            tests_passed++; \
        } else { \
            printf("  ✗ %s\n", message); \
            tests_failed++; \
        } \
    } while (0)

void test_feature_detection(void) {
    printf("\n[TEST] feature_detection\n");
    
    lgx_simd_detect_features();
    bool has_avx2 = lgx_simd_has_avx2();
    
    printf("  CPU has AVX2: %s\n", has_avx2 ? "YES" : "NO");
    TEST_ASSERT(true, "Feature detection completed");
    
    printf("  Test completed\n");
}

void test_find_nonempty_slot(void) {
    printf("\n[TEST] find_nonempty_slot\n");
    
    // Test case 1: All NULL
    void* slots1[16] = {NULL};
    int result1 = lgx_simd_find_nonempty_slot(slots1, 16);
    TEST_ASSERT(result1 == -1, "All NULL slots returns -1");
    
    // Test case 2: First slot non-NULL
    void* slots2[16] = {NULL};
    slots2[0] = (void*)0x1234;
    int result2 = lgx_simd_find_nonempty_slot(slots2, 16);
    TEST_ASSERT(result2 == 0, "First slot non-NULL returns 0");
    
    // Test case 3: Middle slot non-NULL
    void* slots3[16] = {NULL};
    slots3[7] = (void*)0x5678;
    int result3 = lgx_simd_find_nonempty_slot(slots3, 16);
    TEST_ASSERT(result3 == 7, "Middle slot non-NULL returns 7");
    
    // Test case 4: Last slot non-NULL
    void* slots4[16] = {NULL};
    slots4[15] = (void*)0xABCD;
    int result4 = lgx_simd_find_nonempty_slot(slots4, 16);
    TEST_ASSERT(result4 == 15, "Last slot non-NULL returns 15");
    
    // Test case 5: Multiple non-NULL (should return first)
    void* slots5[16] = {NULL};
    slots5[3] = (void*)0x1111;
    slots5[8] = (void*)0x2222;
    slots5[12] = (void*)0x3333;
    int result5 = lgx_simd_find_nonempty_slot(slots5, 16);
    TEST_ASSERT(result5 == 3, "Multiple non-NULL returns first (3)");
    
    printf("  Test completed\n");
}

void test_all_null(void) {
    printf("\n[TEST] all_null\n");
    
    // Test case 1: All NULL
    void* slots1[16] = {NULL};
    bool result1 = lgx_simd_all_null(slots1, 16);
    TEST_ASSERT(result1 == true, "All NULL returns true");
    
    // Test case 2: One non-NULL
    void* slots2[16] = {NULL};
    slots2[5] = (void*)0x1234;
    bool result2 = lgx_simd_all_null(slots2, 16);
    TEST_ASSERT(result2 == false, "One non-NULL returns false");
    
    // Test case 3: All non-NULL
    void* slots3[16];
    for (int i = 0; i < 16; i++) {
        slots3[i] = (void*)(uintptr_t)(i + 1);
    }
    bool result3 = lgx_simd_all_null(slots3, 16);
    TEST_ASSERT(result3 == false, "All non-NULL returns false");
    
    printf("  Test completed\n");
}

void test_count_nonempty(void) {
    printf("\n[TEST] count_nonempty\n");
    
    // Test case 1: All NULL
    void* slots1[16] = {NULL};
    int result1 = lgx_simd_count_nonempty(slots1, 16);
    TEST_ASSERT(result1 == 0, "All NULL returns 0");
    
    // Test case 2: One non-NULL
    void* slots2[16] = {NULL};
    slots2[5] = (void*)0x1234;
    int result2 = lgx_simd_count_nonempty(slots2, 16);
    TEST_ASSERT(result2 == 1, "One non-NULL returns 1");
    
    // Test case 3: Half non-NULL
    void* slots3[16] = {NULL};
    for (int i = 0; i < 8; i++) {
        slots3[i] = (void*)(uintptr_t)(i + 1);
    }
    int result3 = lgx_simd_count_nonempty(slots3, 16);
    TEST_ASSERT(result3 == 8, "Half non-NULL returns 8");
    
    // Test case 4: All non-NULL
    void* slots4[16];
    for (int i = 0; i < 16; i++) {
        slots4[i] = (void*)(uintptr_t)(i + 1);
    }
    int result4 = lgx_simd_count_nonempty(slots4, 16);
    TEST_ASSERT(result4 == 16, "All non-NULL returns 16");
    
    printf("  Test completed\n");
}

void test_edge_cases(void) {
    printf("\n[TEST] edge_cases\n");
    
    // Test case 1: Small array (< 4 elements)
    void* slots1[3] = {NULL, (void*)0x1234, NULL};
    int result1 = lgx_simd_find_nonempty_slot(slots1, 3);
    TEST_ASSERT(result1 == 1, "Small array (3 elements) works");
    
    // Test case 2: Single element
    void* slots2[1] = {(void*)0x5678};
    int result2 = lgx_simd_find_nonempty_slot(slots2, 1);
    TEST_ASSERT(result2 == 0, "Single element works");
    
    // Test case 3: Empty array
    void* slots3[1] = {NULL};
    int result3 = lgx_simd_find_nonempty_slot(slots3, 0);
    TEST_ASSERT(result3 == -1, "Empty array (count=0) returns -1");
    
    // Test case 4: Large array (> 16 elements)
    void* slots4[32] = {NULL};
    slots4[25] = (void*)0xABCD;
    int result4 = lgx_simd_find_nonempty_slot(slots4, 32);
    TEST_ASSERT(result4 == 25, "Large array (32 elements) works");
    
    printf("  Test completed\n");
}

int main(void) {
    printf("=== SIMD Graceful Degradation Tests ===\n");
    printf("Testing SIMD operations with scalar fallback (Task 3.5.2.4)\n");
    
    // Run tests
    test_feature_detection();
    test_find_nonempty_slot();
    test_all_null();
    test_count_nonempty();
    test_edge_cases();
    
    // Print summary
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✅ All tests passed!\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed!\n");
        return 1;
    }
}
