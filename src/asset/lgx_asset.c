/**
 * LGX Asset Pipeline v2.1 — Core Implementation
 *
 * File loading, search paths, hot reload (inotify), compression.
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under Apache License 2.0
 */

#define _GNU_SOURCE
#include "lgx_asset.h"
#include "lgx_runtime.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <pthread.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DEFAULT_MAX_ASSETS       1024
#define DEFAULT_MAX_SEARCH_PATHS 16
#define MAX_PATH_LEN             512
#define INOTIFY_BUF_SIZE         4096

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Asset
 * ═══════════════════════════════════════════════════════════════════════════ */

struct lgx_asset {
    char              path[MAX_PATH_LEN];
    char              resolved[MAX_PATH_LEN];
    lgx_asset_type_t  type;
    lgx_asset_state_t state;
    void*             data;
    size_t            size;
    lgx_asset_manager_t* manager;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Manager
 * ═══════════════════════════════════════════════════════════════════════════ */

struct lgx_asset_manager {
    /* Search paths */
    char**    search_paths;
    uint32_t  path_count;
    uint32_t  max_paths;

    /* Asset registry */
    lgx_asset_t** assets;
    uint32_t  asset_count;
    uint32_t  max_assets;

    /* Hot reload */
    int       inotify_fd;
    lgx_asset_reload_fn reload_cb;
    void*     reload_ctx;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * File I/O Helper
 * ═══════════════════════════════════════════════════════════════════════════ */

static int read_file(const char* path, void** out_data, size_t* out_size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return -1; }

    size_t fsize = (size_t)st.st_size;
    void* buf = malloc(fsize + 1);  /* +1 for null terminator on text */
    if (!buf) { close(fd); return -1; }

    size_t total = 0;
    while (total < fsize) {
        ssize_t n = read(fd, (char*)buf + total, fsize - total);
        if (n <= 0) { free(buf); close(fd); return -1; }
        total += (size_t)n;
    }
    close(fd);

    ((char*)buf)[fsize] = '\0';  /* Null-terminate for text assets */
    *out_data = buf;
    *out_size = fsize;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Path Resolution
 * ═══════════════════════════════════════════════════════════════════════════ */

static int resolve_path(lgx_asset_manager_t* mgr, const char* path, char* out, size_t out_len) {
    /* Try absolute or relative path first */
    if (access(path, R_OK) == 0) {
        strncpy(out, path, out_len - 1);
        out[out_len - 1] = '\0';
        return 0;
    }

    /* Search through registered paths */
    for (uint32_t i = 0; i < mgr->path_count; i++) {
        snprintf(out, out_len, "%s/%s", mgr->search_paths[i], path);
        if (access(out, R_OK) == 0) return 0;
    }

    return -1;  /* Not found */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Manager Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_asset_manager_t* lgx_asset_manager_create(const lgx_asset_config_t* config) {
    lgx_asset_manager_t* mgr = calloc(1, sizeof(*mgr));
    if (!mgr) return NULL;

    mgr->max_assets = (config && config->max_assets > 0)
                      ? config->max_assets : DEFAULT_MAX_ASSETS;
    mgr->max_paths  = (config && config->max_search_paths > 0)
                      ? config->max_search_paths : DEFAULT_MAX_SEARCH_PATHS;

    mgr->assets = calloc(mgr->max_assets, sizeof(lgx_asset_t*));
    if (!mgr->assets) { free(mgr); return NULL; }

    mgr->search_paths = calloc(mgr->max_paths, sizeof(char*));
    if (!mgr->search_paths) { free(mgr->assets); free(mgr); return NULL; }

    /* Initialize inotify */
    mgr->inotify_fd = inotify_init1(IN_NONBLOCK);
    if (mgr->inotify_fd < 0) {
        printf("[LGX ASSET] Warning: inotify not available — hot reload disabled\n");
        mgr->inotify_fd = -1;
    }

    printf("[LGX ASSET] Manager created: max_assets=%u, max_paths=%u, inotify=%s\n",
           mgr->max_assets, mgr->max_paths,
           mgr->inotify_fd >= 0 ? "enabled" : "disabled");
    return mgr;
}

void lgx_asset_manager_destroy(lgx_asset_manager_t* mgr) {
    if (!mgr) return;

    /* Unload all assets */
    for (uint32_t i = 0; i < mgr->asset_count; i++) {
        if (mgr->assets[i]) {
            free(mgr->assets[i]->data);
            free(mgr->assets[i]);
        }
    }
    free(mgr->assets);

    /* Free search paths */
    for (uint32_t i = 0; i < mgr->path_count; i++) {
        free(mgr->search_paths[i]);
    }
    free(mgr->search_paths);

    if (mgr->inotify_fd >= 0) close(mgr->inotify_fd);
    free(mgr);
    printf("[LGX ASSET] Manager destroyed\n");
}

lgx_result_t lgx_asset_manager_update(lgx_asset_manager_t* mgr) {
    if (!mgr) return LGX_ERROR_INVALID_PARAM;
    if (mgr->inotify_fd < 0) return LGX_SUCCESS;

    char buf[INOTIFY_BUF_SIZE];
    ssize_t len = read(mgr->inotify_fd, buf, sizeof(buf));
    if (len <= 0) return LGX_SUCCESS;

    /* Process inotify events */
    size_t offset = 0;
    while (offset < (size_t)len) {
        struct inotify_event* event = (struct inotify_event*)(buf + offset);
        if (event->len > 0 && mgr->reload_cb) {
            mgr->reload_cb(event->name, mgr->reload_ctx);
        }
        offset += sizeof(struct inotify_event) + event->len;
    }

    return LGX_SUCCESS;
}

lgx_result_t lgx_asset_manager_add_path(lgx_asset_manager_t* mgr, const char* path) {
    if (!mgr || !path) return LGX_ERROR_INVALID_PARAM;
    if (mgr->path_count >= mgr->max_paths) return LGX_ERROR_INVALID_PARAM;

    mgr->search_paths[mgr->path_count] = strdup(path);
    if (!mgr->search_paths[mgr->path_count]) return LGX_ERROR_INVALID_PARAM;
    mgr->path_count++;

    printf("[LGX ASSET] Search path added: %s\n", path);
    return LGX_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Loading
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_asset_t* lgx_asset_load(lgx_asset_manager_t* mgr, const char* path,
                            lgx_asset_type_t type) {
    if (!mgr || !path) return NULL;
    if (mgr->asset_count >= mgr->max_assets) return NULL;

    lgx_asset_t* asset = calloc(1, sizeof(*asset));
    if (!asset) return NULL;

    strncpy(asset->path, path, MAX_PATH_LEN - 1);
    asset->type = type;
    asset->manager = mgr;

    /* Resolve path */
    if (resolve_path(mgr, path, asset->resolved, MAX_PATH_LEN) < 0) {
        asset->state = LGX_ASSET_ERROR;
        printf("[LGX ASSET] Not found: %s\n", path);
        /* Still register the asset so it can be queried */
        mgr->assets[mgr->asset_count++] = asset;
        return asset;
    }

    /* Read file */
    if (read_file(asset->resolved, &asset->data, &asset->size) < 0) {
        asset->state = LGX_ASSET_ERROR;
        printf("[LGX ASSET] Read error: %s\n", asset->resolved);
    } else {
        asset->state = LGX_ASSET_READY;
    }

    mgr->assets[mgr->asset_count++] = asset;
    return asset;
}

lgx_asset_t* lgx_asset_load_async(lgx_asset_manager_t* mgr, const char* path,
                                  lgx_asset_type_t type) {
    /* For now, async is implemented as sync with LOADING state transition */
    if (!mgr || !path) return NULL;

    lgx_asset_t* asset = lgx_asset_load(mgr, path, type);
    return asset;
}

void lgx_asset_unload(lgx_asset_t* asset) {
    if (!asset) return;
    free(asset->data);
    asset->data = NULL;
    asset->size = 0;
    asset->state = LGX_ASSET_UNLOADED;
}

lgx_result_t lgx_asset_reload(lgx_asset_t* asset) {
    if (!asset) return LGX_ERROR_INVALID_PARAM;
    if (asset->resolved[0] == '\0') return LGX_ERROR_INVALID_PARAM;

    /* Free old data */
    free(asset->data);
    asset->data = NULL;
    asset->size = 0;

    /* Re-read */
    if (read_file(asset->resolved, &asset->data, &asset->size) < 0) {
        asset->state = LGX_ASSET_ERROR;
        return LGX_ERROR_INVALID_PARAM;
    }

    asset->state = LGX_ASSET_READY;
    return LGX_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Query
 * ═══════════════════════════════════════════════════════════════════════════ */

const void* lgx_asset_get_data(lgx_asset_t* asset) {
    if (!asset || asset->state != LGX_ASSET_READY) return NULL;
    return asset->data;
}

size_t lgx_asset_get_size(lgx_asset_t* asset) {
    if (!asset) return 0;
    return asset->size;
}

lgx_asset_state_t lgx_asset_get_state(lgx_asset_t* asset) {
    if (!asset) return LGX_ASSET_ERROR;
    return asset->state;
}

lgx_asset_type_t lgx_asset_get_type(lgx_asset_t* asset) {
    if (!asset) return LGX_ASSET_BINARY;
    return asset->type;
}

const char* lgx_asset_get_path(lgx_asset_t* asset) {
    if (!asset) return NULL;
    return asset->path;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Hot Reload
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_result_t lgx_asset_watch(lgx_asset_manager_t* mgr, const char* path) {
    if (!mgr || !path) return LGX_ERROR_INVALID_PARAM;
    if (mgr->inotify_fd < 0) return LGX_ERROR_INVALID_PARAM;

    int wd = inotify_add_watch(mgr->inotify_fd, path,
                               IN_MODIFY | IN_CREATE | IN_DELETE);
    if (wd < 0) {
        printf("[LGX ASSET] Watch failed for %s: %s\n", path, strerror(errno));
        return LGX_ERROR_INVALID_PARAM;
    }

    printf("[LGX ASSET] Watching: %s (wd=%d)\n", path, wd);
    return LGX_SUCCESS;
}

lgx_result_t lgx_asset_set_reload_callback(lgx_asset_manager_t* mgr,
                                           lgx_asset_reload_fn cb, void* user_ctx) {
    if (!mgr) return LGX_ERROR_INVALID_PARAM;
    mgr->reload_cb = cb;
    mgr->reload_ctx = user_ctx;
    return LGX_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Compression
 *
 * Simple format: stream of commands
 *   Literal:  [0x00] [length_u16] [bytes...]
 *   Match:    [offset_u16 | 0x8000] [length_u16]
 *
 * For simplicity, this version does literal-only with run-length encoding
 * on repeated bytes — good enough for typical game assets.
 *
 * Format: [tag] [data...]
 *   tag 0x00 + u16 len: literal run
 *   tag 0x01 + u8 byte + u16 count: repeated byte run
 * ═══════════════════════════════════════════════════════════════════════════ */

int lgx_asset_compress(const void* data, size_t size, void* out, size_t max_out) {
    if (!data || !out || size == 0 || max_out == 0) return -1;

    const uint8_t* src = (const uint8_t*)data;
    uint8_t* dst = (uint8_t*)out;
    size_t si = 0, di = 0;

    while (si < size) {
        /* Check for repeated byte run */
        uint8_t cur = src[si];
        size_t run = 1;
        while (si + run < size && src[si + run] == cur && run < 65535) {
            run++;
        }

        if (run >= 4) {
            /* RLE: tag=0x01, byte, count_u16 */
            if (di + 4 > max_out) return -1;
            dst[di++] = 0x01;
            dst[di++] = cur;
            dst[di++] = (uint8_t)(run & 0xFF);
            dst[di++] = (uint8_t)((run >> 8) & 0xFF);
            si += run;
        } else {
            /* Literal run: collect until we hit a repeated run */
            size_t lit_start = si;
            while (si < size) {
                if (si + 3 < size &&
                    src[si] == src[si+1] && src[si+1] == src[si+2] && src[si+2] == src[si+3]) {
                    break;  /* Start of RLE run */
                }
                si++;
                if (si - lit_start >= 65535) break;
            }

            size_t lit_len = si - lit_start;
            if (di + 3 + lit_len > max_out) return -1;
            dst[di++] = 0x00;
            dst[di++] = (uint8_t)(lit_len & 0xFF);
            dst[di++] = (uint8_t)((lit_len >> 8) & 0xFF);
            memcpy(dst + di, src + lit_start, lit_len);
            di += lit_len;
        }
    }

    return (int)di;
}

int lgx_asset_decompress(const void* data, size_t size, void* out, size_t max_out) {
    if (!data || !out || size == 0 || max_out == 0) return -1;

    const uint8_t* src = (const uint8_t*)data;
    uint8_t* dst = (uint8_t*)out;
    size_t si = 0, di = 0;

    while (si < size) {
        uint8_t tag = src[si++];

        if (tag == 0x00) {
            /* Literal run */
            if (si + 2 > size) return -1;
            uint16_t len = src[si] | ((uint16_t)src[si+1] << 8);
            si += 2;
            if (si + len > size || di + len > max_out) return -1;
            memcpy(dst + di, src + si, len);
            si += len;
            di += len;
        } else if (tag == 0x01) {
            /* RLE run */
            if (si + 3 > size) return -1;
            uint8_t byte = src[si++];
            uint16_t count = src[si] | ((uint16_t)src[si+1] << 8);
            si += 2;
            if (di + count > max_out) return -1;
            memset(dst + di, byte, count);
            di += count;
        } else {
            return -1;  /* Unknown tag */
        }
    }

    return (int)di;
}
