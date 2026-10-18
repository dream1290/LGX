#include <stdio.h>
static int tests_failed = 0;
#define TEST(n) printf("[TEST] %s\n", #n); if (test_##n()) { } else { tests_failed++; }
static int test_a(void) { return 1; }
int main(void) { TEST(a); printf("failed=%d\n", tests_failed); return 0; }
