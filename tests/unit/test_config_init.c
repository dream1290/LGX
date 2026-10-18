#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);

    /* calloc: every field that lgx_runtime_init reads unguarded or
       behind a "> 0" check must start at zero. */
    assert(config->frame_arena_size == 0);
    assert(config->frame_arena_max_size == 0);

    lgx_config_destroy(config);
    printf("test_config_init: OK\n");
    return 0;
}
