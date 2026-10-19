/* Tests/unit: leak detector init/add/shutdown cycle.
 *
 * Regression guard introduced with the mutex-order fix (Task B8a):
 * lgx_leak_detector_init() used to memset the whole state AFTER
 * pthread_mutex_init(), wiping the mutex it had just initialised.
 * Zeroed memory happens to be a valid default mutex on glibc, so this
 * cycle passes pre-fix on this platform BY LUCK -- the fix is justified
 * by the standard (UB), not by a red test here.
 */
#include "lgx_runtime.h"
#include <stdio.h>

int main(void) {
    int failures = 0;

    lgx_result_t rc = lgx_leak_detector_init();
    if (rc != LGX_SUCCESS) { printf("FAIL: first init rc=%d\n", rc); failures++; }

    rc = lgx_leak_detector_add_sample(1024, 512, 0, 512);
    if (rc != LGX_SUCCESS) { printf("FAIL: add_sample rc=%d\n", rc); failures++; }

    rc = lgx_leak_detector_shutdown();
    if (rc != LGX_SUCCESS) { printf("FAIL: shutdown rc=%d\n", rc); failures++; }

    /* Full cycle restarts cleanly with a re-initialised mutex. */
    rc = lgx_leak_detector_init();
    if (rc != LGX_SUCCESS) { printf("FAIL: second init rc=%d\n", rc); failures++; }

    rc = lgx_leak_detector_add_sample(2048, 1024, 0, 1024);
    if (rc != LGX_SUCCESS) { printf("FAIL: second add_sample rc=%d\n", rc); failures++; }

    rc = lgx_leak_detector_shutdown();
    if (rc != LGX_SUCCESS) { printf("FAIL: second shutdown rc=%d\n", rc); failures++; }

    if (failures) { printf("test_leak_detector_cycle: %d FAILURES\n", failures); return 1; }
    printf("test_leak_detector_cycle: 6 steps OK\n");
    return 0;
}
