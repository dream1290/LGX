#ifndef LGX_RUNTIME_H
#define LGX_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Result codes
typedef enum lgx_result {
    LGX_SUCCESS = 0,
    LGX_ERROR_INVALID_PARAM = 1,
    LGX_ERROR_NOT_INITIALIZED = 2,
    LGX_ERROR_OUT_OF_MEMORY = 3,
} lgx_result_t;

// Version information
typedef struct lgx_version {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} lgx_version_t;

// Runtime initialization and shutdown
lgx_result_t lgx_runtime_init(void);
lgx_result_t lgx_runtime_shutdown(void);

// Version query
lgx_version_t lgx_runtime_get_version(void);

// Memory allocation (Phase 0: simple malloc wrapper)
void* lgx_alloc(size_t size);
void lgx_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif // LGX_RUNTIME_H
