/**
 * LGX Asset Pipeline v2.1 — Public API
 *
 * Asset loading, hot reloading, streaming, compression.
 * Designed for game content management.
 *
 * Requires: lgx_runtime (v1.0)
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under the Apache License, Version 2.0
 */

#ifndef LGX_ASSET_H
#define LGX_ASSET_H

#include "lgx_types.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Version
 * ═══════════════════════════════════════════════════════════════════════════ */

#define LGX_ASSET_VERSION_MAJOR  2
#define LGX_ASSET_VERSION_MINOR  1
#define LGX_ASSET_VERSION_PATCH  0

/* ═══════════════════════════════════════════════════════════════════════════
 * Opaque Handles
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Asset manager — registry, search paths, hot reload watcher */
typedef struct lgx_asset_manager  lgx_asset_manager_t;

/** Loaded asset — data, size, type, state */
typedef struct lgx_asset           lgx_asset_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Enums
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    LGX_ASSET_BINARY = 0,
    LGX_ASSET_TEXT,
    LGX_ASSET_IMAGE,
    LGX_ASSET_AUDIO,
    LGX_ASSET_SHADER,
    LGX_ASSET_MESH,
} lgx_asset_type_t;

typedef enum {
    LGX_ASSET_UNLOADED = 0,
    LGX_ASSET_LOADING,
    LGX_ASSET_READY,
    LGX_ASSET_ERROR,
} lgx_asset_state_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct lgx_asset_config {
    size_t   struct_size;       /**< Must be sizeof(lgx_asset_config_t) */
    uint32_t max_assets;        /**< Max loaded assets (default 1024) */
    uint32_t max_search_paths;  /**< Max search directories (default 16) */
} lgx_asset_config_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Callback
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Called when a watched file changes. */
typedef void (*lgx_asset_reload_fn)(const char* path, void* user_ctx);

/* ═══════════════════════════════════════════════════════════════════════════
 * Manager Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Create asset manager. NULL config = defaults. */
lgx_asset_manager_t* lgx_asset_manager_create(const lgx_asset_config_t* config);

/** Destroy manager and unload all assets. NULL-safe. */
void lgx_asset_manager_destroy(lgx_asset_manager_t* mgr);

/** Poll for hot-reload events. Call once per frame. */
lgx_result_t lgx_asset_manager_update(lgx_asset_manager_t* mgr);

/** Add a search directory for asset resolution. */
lgx_result_t lgx_asset_manager_add_path(lgx_asset_manager_t* mgr, const char* path);

/* ═══════════════════════════════════════════════════════════════════════════
 * Loading
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Load asset synchronously. Returns NULL on error. */
lgx_asset_t* lgx_asset_load(lgx_asset_manager_t* mgr, const char* path,
                            lgx_asset_type_t type);

/** Begin async load. Query state with lgx_asset_get_state(). */
lgx_asset_t* lgx_asset_load_async(lgx_asset_manager_t* mgr, const char* path,
                                  lgx_asset_type_t type);

/** Unload and free asset data. NULL-safe. */
void lgx_asset_unload(lgx_asset_t* asset);

/** Force reload from disk. */
lgx_result_t lgx_asset_reload(lgx_asset_t* asset);

/* ═══════════════════════════════════════════════════════════════════════════
 * Query
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Get pointer to loaded data. NULL if not ready. */
const void* lgx_asset_get_data(lgx_asset_t* asset);

/** Get data size in bytes. */
size_t lgx_asset_get_size(lgx_asset_t* asset);

/** Get current asset state. */
lgx_asset_state_t lgx_asset_get_state(lgx_asset_t* asset);

/** Get asset type. */
lgx_asset_type_t lgx_asset_get_type(lgx_asset_t* asset);

/** Get original path string. */
const char* lgx_asset_get_path(lgx_asset_t* asset);

/* ═══════════════════════════════════════════════════════════════════════════
 * Hot Reload
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Watch a directory for changes (uses inotify). */
lgx_result_t lgx_asset_watch(lgx_asset_manager_t* mgr, const char* path);

/** Set callback for when a watched file changes. */
lgx_result_t lgx_asset_set_reload_callback(lgx_asset_manager_t* mgr,
                                           lgx_asset_reload_fn cb, void* user_ctx);

/* ═══════════════════════════════════════════════════════════════════════════
 * Compression
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Fast LZ4-style compression.
 * @return Compressed size, or -1 on error.
 */
int lgx_asset_compress(const void* data, size_t size, void* out, size_t max_out);

/**
 * Decompress data compressed by lgx_asset_compress.
 * @return Decompressed size, or -1 on error.
 */
int lgx_asset_decompress(const void* data, size_t size, void* out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* LGX_ASSET_H */
