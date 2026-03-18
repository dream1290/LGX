# LGX Asset Pipeline v2.1 — API Reference

> File loading, hot reloading, search paths, compression.

**Requires:** `lgx_runtime` (v1.0)
**Header:** `#include "lgx_asset.h"`
**Library:** `-llgx_asset -lpthread`

---

## Manager Lifecycle

| Function | Description |
|----------|-------------|
| `lgx_asset_manager_create(config)` | Create manager. NULL = defaults (1024 assets, 16 paths). |
| `lgx_asset_manager_destroy(mgr)` | Destroy and unload all. NULL-safe. |
| `lgx_asset_manager_update(mgr)` | Poll hot-reload events (call per frame). |
| `lgx_asset_manager_add_path(mgr, path)` | Add search directory (first-match priority). |

---

## Loading

| Function | Description |
|----------|-------------|
| `lgx_asset_load(mgr, path, type)` | Sync load. Returns asset (ERROR state if not found). |
| `lgx_asset_load_async(mgr, path, type)` | Begin async load. |
| `lgx_asset_unload(asset)` | Free data, set UNLOADED. NULL-safe. |
| `lgx_asset_reload(asset)` | Re-read from disk. |

---

## Query

| Function | Description |
|----------|-------------|
| `lgx_asset_get_data(asset)` | Pointer to data (NULL if not READY). |
| `lgx_asset_get_size(asset)` | Size in bytes. |
| `lgx_asset_get_state(asset)` | `UNLOADED`, `LOADING`, `READY`, `ERROR`. |
| `lgx_asset_get_type(asset)` | `BINARY`, `TEXT`, `IMAGE`, `AUDIO`, `SHADER`, `MESH`. |
| `lgx_asset_get_path(asset)` | Original path string. |

---

## Hot Reload

| Function | Description |
|----------|-------------|
| `lgx_asset_watch(mgr, path)` | Watch directory via inotify. |
| `lgx_asset_set_reload_callback(mgr, cb, ctx)` | Callback on file change. |

---

## Compression

| Function | Description |
|----------|-------------|
| `lgx_asset_compress(data, size, out, max)` | RLE compress (literal + repeat runs). |
| `lgx_asset_decompress(data, size, out, max)` | Decompress. |

---

## Features

- **File I/O:** POSIX `open()`/`read()`, null-terminated text assets
- **Search paths:** Ordered, first-match resolution
- **Hot reload:** Linux inotify (IN_MODIFY | IN_CREATE | IN_DELETE)
- **Compression:** Literal/RLE byte encoding, good for repetitive data
- **No extra deps:** POSIX + pthreads only
