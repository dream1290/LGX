# LGX Networking Module v2.0 — API Reference

> UDP sockets, connection management, serialization, delta compression.

**Requires:** `lgx_runtime` (v1.0)
**Header:** `#include "lgx_net.h"`
**Library:** `-llgx_net`

---

## Socket Lifecycle

| Function | Description |
|----------|-------------|
| `lgx_net_socket_create(config)` | Create UDP socket. NULL = defaults (32 peers, 5s timeout, 60Hz). |
| `lgx_net_socket_destroy(sock)` | Close socket. NULL-safe. |
| `lgx_net_socket_bind(sock, port)` | Bind to local port (0 = ephemeral). |
| `lgx_net_socket_send(sock, addr, data, len)` | Send raw UDP packet. Returns bytes sent. |
| `lgx_net_socket_recv(sock, addr, buf, max)` | Non-blocking receive. Returns bytes, 0 if empty. |

---

## Connection Management

| Function | Description |
|----------|-------------|
| `lgx_net_connect(sock, host, port)` | Register peer. Returns connection ID. |
| `lgx_net_disconnect(sock, conn_id)` | Remove peer. |
| `lgx_net_update(sock)` | Process timeouts (call once per tick). |
| `lgx_net_get_connection_count(sock)` | Active peer count. |
| `lgx_net_get_rtt_ms(sock, conn_id)` | Round-trip time (ms). |

---

## Serialization Stream

| Function | Description |
|----------|-------------|
| `lgx_net_stream_create(buf, size)` | Create write stream. |
| `lgx_net_stream_create_read(data, size)` | Create read stream. |
| `lgx_net_stream_destroy(stream)` | Destroy. NULL-safe. |
| `lgx_net_stream_write_u8/u16/u32/f32(s, val)` | Write typed value (little-endian). |
| `lgx_net_stream_read_u8/u16/u32/f32(s)` | Read typed value. |
| `lgx_net_stream_write_string(s, str, max)` | Length-prefixed string write. |
| `lgx_net_stream_read_string(s, buf, max)` | String read. |
| `lgx_net_stream_bytes_written(s)` | Bytes consumed. |

---

## Delta Compression

| Function | Description |
|----------|-------------|
| `lgx_net_delta_encode(prev, curr, size, out, max)` | XOR + RLE encode. Returns compressed bytes. |
| `lgx_net_delta_decode(prev, delta, len, out, size)` | Decode delta to reconstruct state. |

**Format:** XOR each byte → zero runs are RLE-compressed (`0x00` + count), non-zero bytes written directly.

---

## Features

- **Transport:** POSIX UDP, non-blocking, `SO_REUSEADDR`
- **Connections:** Fixed peer table with timeout-based disconnection
- **Serialization:** Byte-aligned, little-endian, bounds-checked
- **Delta:** XOR + RLE, efficient for game state snapshots
- **No extra deps:** POSIX sockets only
