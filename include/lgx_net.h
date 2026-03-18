/**
 * LGX Networking Module v2.0 — Public API
 *
 * UDP sockets, connection management, serialization streams,
 * delta compression. Designed for real-time multiplayer.
 *
 * Requires: lgx_runtime (v1.0)
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under the Apache License, Version 2.0
 */

#ifndef LGX_NET_H
#define LGX_NET_H

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

#define LGX_NET_VERSION_MAJOR  2
#define LGX_NET_VERSION_MINOR  0
#define LGX_NET_VERSION_PATCH  0

/* ═══════════════════════════════════════════════════════════════════════════
 * Opaque Handles
 * ═══════════════════════════════════════════════════════════════════════════ */

/** UDP socket with connection tracking */
typedef struct lgx_net_socket   lgx_net_socket_t;

/** Serialization stream (byte-packed read/write) */
typedef struct lgx_net_stream   lgx_net_stream_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Network Address
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct lgx_net_addr {
    uint32_t ip;        /**< IPv4 address in network byte order */
    uint16_t port;      /**< Port in host byte order */
} lgx_net_addr_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct lgx_net_config {
    size_t   struct_size;       /**< Must be sizeof(lgx_net_config_t) */
    uint32_t max_connections;   /**< Max simultaneous peers (default 32) */
    uint32_t timeout_ms;        /**< Connection timeout in ms (default 5000) */
    uint32_t tick_rate;         /**< Server tick rate Hz (default 60) */
} lgx_net_config_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Socket Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create a UDP socket. NULL config = defaults (32 connections, 5s timeout, 60Hz).
 */
lgx_net_socket_t* lgx_net_socket_create(const lgx_net_config_t* config);

/** Destroy socket and free all resources. NULL-safe. */
void lgx_net_socket_destroy(lgx_net_socket_t* sock);

/** Bind socket to a local port. Returns LGX_SUCCESS or error. */
lgx_result_t lgx_net_socket_bind(lgx_net_socket_t* sock, uint16_t port);

/**
 * Send raw data to an address.
 * @return Bytes sent, or -1 on error.
 */
int lgx_net_socket_send(lgx_net_socket_t* sock, const lgx_net_addr_t* addr,
                        const void* data, size_t len);

/**
 * Receive a packet (non-blocking).
 * @param addr  Filled with sender address on success.
 * @return Bytes received, 0 if no data, -1 on error.
 */
int lgx_net_socket_recv(lgx_net_socket_t* sock, lgx_net_addr_t* addr,
                        void* buf, size_t max_len);

/* ═══════════════════════════════════════════════════════════════════════════
 * Connection Management
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Register a connection to a remote peer.
 * @return Connection ID (≥ 0) or -1 if table is full.
 */
int lgx_net_connect(lgx_net_socket_t* sock, const char* host, uint16_t port);

/** Disconnect a peer by connection ID. */
lgx_result_t lgx_net_disconnect(lgx_net_socket_t* sock, int conn_id);

/** Process timeouts and keepalives. Call once per tick. */
lgx_result_t lgx_net_update(lgx_net_socket_t* sock);

/** Get number of active connections. */
uint32_t lgx_net_get_connection_count(lgx_net_socket_t* sock);

/** Get round-trip time for a connection in milliseconds. */
double lgx_net_get_rtt_ms(lgx_net_socket_t* sock, int conn_id);

/* ═══════════════════════════════════════════════════════════════════════════
 * Serialization Stream
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Create a write stream backed by a buffer. */
lgx_net_stream_t* lgx_net_stream_create(void* buf, size_t size);

/** Create a read stream from existing data. */
lgx_net_stream_t* lgx_net_stream_create_read(const void* data, size_t size);

/** Destroy stream. NULL-safe. */
void lgx_net_stream_destroy(lgx_net_stream_t* stream);

/** Write typed values. Returns LGX_SUCCESS or overflow error. */
lgx_result_t lgx_net_stream_write_u8(lgx_net_stream_t* s, uint8_t val);
lgx_result_t lgx_net_stream_write_u16(lgx_net_stream_t* s, uint16_t val);
lgx_result_t lgx_net_stream_write_u32(lgx_net_stream_t* s, uint32_t val);
lgx_result_t lgx_net_stream_write_f32(lgx_net_stream_t* s, float val);

/** Write a length-prefixed string. max_len includes null terminator. */
lgx_result_t lgx_net_stream_write_string(lgx_net_stream_t* s, const char* str, uint16_t max_len);

/** Read typed values. Returns 0 on underflow. */
uint8_t  lgx_net_stream_read_u8(lgx_net_stream_t* s);
uint16_t lgx_net_stream_read_u16(lgx_net_stream_t* s);
uint32_t lgx_net_stream_read_u32(lgx_net_stream_t* s);
float    lgx_net_stream_read_f32(lgx_net_stream_t* s);

/** Read a length-prefixed string into buf. Returns bytes read. */
int lgx_net_stream_read_string(lgx_net_stream_t* s, char* buf, uint16_t max_len);

/** Get number of bytes written/read so far. */
size_t lgx_net_stream_bytes_written(lgx_net_stream_t* s);

/* ═══════════════════════════════════════════════════════════════════════════
 * Delta Compression
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * XOR-based delta encode between previous and current state.
 * Output is XOR'd bytes with RLE on zero runs.
 *
 * @param prev     Previous state snapshot
 * @param curr     Current state snapshot
 * @param size     State size in bytes
 * @param out      Output buffer
 * @param max_out  Output buffer capacity
 * @return Bytes written to out, or -1 on error.
 */
int lgx_net_delta_encode(const void* prev, const void* curr, size_t size,
                         void* out, size_t max_out);

/**
 * Decode a delta against a previous state to reconstruct current state.
 *
 * @param prev      Previous state snapshot
 * @param delta     Delta-encoded data
 * @param delta_len Length of delta data
 * @param out       Output buffer (must be at least prev state size)
 * @param out_size  Output buffer size
 * @return Bytes written to out, or -1 on error.
 */
int lgx_net_delta_decode(const void* prev, const void* delta, size_t delta_len,
                         void* out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* LGX_NET_H */
