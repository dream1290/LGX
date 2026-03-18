/**
 * LGX Networking Module v2.0 — Core Implementation
 *
 * UDP sockets, connection table, serialization streams, delta compression.
 * POSIX sockets only — no external dependencies.
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under Apache License 2.0
 */

#define _GNU_SOURCE
#include "lgx_net.h"
#include "lgx_runtime.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DEFAULT_MAX_CONNECTIONS 32
#define DEFAULT_TIMEOUT_MS      5000
#define DEFAULT_TICK_RATE       60
#define MAX_PACKET_SIZE         1400

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Connection Entry
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    bool            active;
    lgx_net_addr_t  addr;
    uint64_t        last_recv_ms;
    double          rtt_ms;
    uint32_t        seq_out;
    uint32_t        seq_in;
} connection_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Socket
 * ═══════════════════════════════════════════════════════════════════════════ */

struct lgx_net_socket {
    int             fd;
    bool            bound;
    connection_t*   connections;
    uint32_t        max_connections;
    uint32_t        timeout_ms;
    uint32_t        tick_rate;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Stream
 * ═══════════════════════════════════════════════════════════════════════════ */

struct lgx_net_stream {
    uint8_t*    buf;
    size_t      capacity;
    size_t      pos;
    bool        is_read;
    bool        overflow;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Time Helper
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint64_t time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Socket Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_net_socket_t* lgx_net_socket_create(const lgx_net_config_t* config) {
    lgx_net_socket_t* sock = calloc(1, sizeof(*sock));
    if (!sock) return NULL;

    sock->max_connections = (config && config->max_connections > 0)
                            ? config->max_connections : DEFAULT_MAX_CONNECTIONS;
    sock->timeout_ms = (config && config->timeout_ms > 0)
                       ? config->timeout_ms : DEFAULT_TIMEOUT_MS;
    sock->tick_rate = (config && config->tick_rate > 0)
                      ? config->tick_rate : DEFAULT_TICK_RATE;

    sock->connections = calloc(sock->max_connections, sizeof(connection_t));
    if (!sock->connections) { free(sock); return NULL; }

    /* Create UDP socket */
    sock->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock->fd < 0) {
        printf("[LGX NET] Warning: could not create UDP socket (%s) — headless mode\n",
               strerror(errno));
        sock->fd = -1;
    } else {
        /* Set non-blocking */
        int flags = fcntl(sock->fd, F_GETFL, 0);
        if (flags >= 0) fcntl(sock->fd, F_SETFL, flags | O_NONBLOCK);
    }

    printf("[LGX NET] Socket created: fd=%d, max_conn=%u, timeout=%ums, tick=%uHz\n",
           sock->fd, sock->max_connections, sock->timeout_ms, sock->tick_rate);
    return sock;
}

void lgx_net_socket_destroy(lgx_net_socket_t* sock) {
    if (!sock) return;
    if (sock->fd >= 0) close(sock->fd);
    free(sock->connections);
    free(sock);
    printf("[LGX NET] Socket destroyed\n");
}

lgx_result_t lgx_net_socket_bind(lgx_net_socket_t* sock, uint16_t port) {
    if (!sock) return LGX_ERROR_INVALID_PARAM;
    if (sock->fd < 0) return LGX_ERROR_INVALID_PARAM;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    /* Allow address reuse for quick restarts */
    int opt = 1;
    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(sock->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("[LGX NET] Bind to port %u failed: %s\n", port, strerror(errno));
        return LGX_ERROR_INVALID_PARAM;
    }

    sock->bound = true;
    printf("[LGX NET] Bound to port %u\n", port);
    return LGX_SUCCESS;
}

int lgx_net_socket_send(lgx_net_socket_t* sock, const lgx_net_addr_t* addr,
                        const void* data, size_t len) {
    if (!sock || !addr || !data || len == 0) return -1;
    if (sock->fd < 0) return -1;

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = addr->ip;
    sa.sin_port = htons(addr->port);

    ssize_t sent = sendto(sock->fd, data, len, 0,
                          (struct sockaddr*)&sa, sizeof(sa));
    return (int)sent;
}

int lgx_net_socket_recv(lgx_net_socket_t* sock, lgx_net_addr_t* addr,
                        void* buf, size_t max_len) {
    if (!sock || !buf || max_len == 0) return -1;
    if (sock->fd < 0) return -1;

    struct sockaddr_in sa;
    socklen_t sa_len = sizeof(sa);
    memset(&sa, 0, sizeof(sa));

    ssize_t received = recvfrom(sock->fd, buf, max_len, 0,
                                (struct sockaddr*)&sa, &sa_len);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;  /* No data */
        return -1;
    }

    if (addr) {
        addr->ip = sa.sin_addr.s_addr;
        addr->port = ntohs(sa.sin_port);
    }

    return (int)received;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Connection Management
 * ═══════════════════════════════════════════════════════════════════════════ */

int lgx_net_connect(lgx_net_socket_t* sock, const char* host, uint16_t port) {
    if (!sock || !host) return -1;

    /* Resolve host */
    struct hostent* he = gethostbyname(host);
    if (!he) return -1;

    lgx_net_addr_t addr;
    memcpy(&addr.ip, he->h_addr_list[0], sizeof(addr.ip));
    addr.port = port;

    /* Find empty slot */
    for (uint32_t i = 0; i < sock->max_connections; i++) {
        if (!sock->connections[i].active) {
            sock->connections[i].active = true;
            sock->connections[i].addr = addr;
            sock->connections[i].last_recv_ms = time_ms();
            sock->connections[i].rtt_ms = 0.0;
            sock->connections[i].seq_out = 0;
            sock->connections[i].seq_in = 0;
            printf("[LGX NET] Connection %u established to %s:%u\n", i, host, port);
            return (int)i;
        }
    }
    return -1;  /* Full */
}

lgx_result_t lgx_net_disconnect(lgx_net_socket_t* sock, int conn_id) {
    if (!sock || conn_id < 0 || (uint32_t)conn_id >= sock->max_connections)
        return LGX_ERROR_INVALID_PARAM;
    if (!sock->connections[conn_id].active) return LGX_ERROR_INVALID_PARAM;

    sock->connections[conn_id].active = false;
    printf("[LGX NET] Connection %d disconnected\n", conn_id);
    return LGX_SUCCESS;
}

lgx_result_t lgx_net_update(lgx_net_socket_t* sock) {
    if (!sock) return LGX_ERROR_INVALID_PARAM;

    uint64_t now = time_ms();

    for (uint32_t i = 0; i < sock->max_connections; i++) {
        if (!sock->connections[i].active) continue;

        /* Check timeout */
        uint64_t elapsed = now - sock->connections[i].last_recv_ms;
        if (elapsed > sock->timeout_ms) {
            printf("[LGX NET] Connection %u timed out (%lu ms)\n", i, (unsigned long)elapsed);
            sock->connections[i].active = false;
        }
    }

    return LGX_SUCCESS;
}

uint32_t lgx_net_get_connection_count(lgx_net_socket_t* sock) {
    if (!sock) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < sock->max_connections; i++) {
        if (sock->connections[i].active) count++;
    }
    return count;
}

double lgx_net_get_rtt_ms(lgx_net_socket_t* sock, int conn_id) {
    if (!sock || conn_id < 0 || (uint32_t)conn_id >= sock->max_connections)
        return 0.0;
    if (!sock->connections[conn_id].active) return 0.0;
    return sock->connections[conn_id].rtt_ms;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Serialization Stream
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_net_stream_t* lgx_net_stream_create(void* buf, size_t size) {
    if (!buf || size == 0) return NULL;

    lgx_net_stream_t* s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->buf = (uint8_t*)buf;
    s->capacity = size;
    s->pos = 0;
    s->is_read = false;
    s->overflow = false;
    return s;
}

lgx_net_stream_t* lgx_net_stream_create_read(const void* data, size_t size) {
    if (!data || size == 0) return NULL;

    lgx_net_stream_t* s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->buf = (uint8_t*)(uintptr_t)data;  /* const-cast: reads only */
    s->capacity = size;
    s->pos = 0;
    s->is_read = true;
    s->overflow = false;
    return s;
}

void lgx_net_stream_destroy(lgx_net_stream_t* s) {
    free(s);  /* NULL-safe */
}

/* Write helpers */
lgx_result_t lgx_net_stream_write_u8(lgx_net_stream_t* s, uint8_t val) {
    if (!s || s->is_read) return LGX_ERROR_INVALID_PARAM;
    if (s->pos + 1 > s->capacity) { s->overflow = true; return LGX_ERROR_INVALID_PARAM; }
    s->buf[s->pos++] = val;
    return LGX_SUCCESS;
}

lgx_result_t lgx_net_stream_write_u16(lgx_net_stream_t* s, uint16_t val) {
    if (!s || s->is_read) return LGX_ERROR_INVALID_PARAM;
    if (s->pos + 2 > s->capacity) { s->overflow = true; return LGX_ERROR_INVALID_PARAM; }
    s->buf[s->pos++] = (uint8_t)(val & 0xFF);
    s->buf[s->pos++] = (uint8_t)(val >> 8);
    return LGX_SUCCESS;
}

lgx_result_t lgx_net_stream_write_u32(lgx_net_stream_t* s, uint32_t val) {
    if (!s || s->is_read) return LGX_ERROR_INVALID_PARAM;
    if (s->pos + 4 > s->capacity) { s->overflow = true; return LGX_ERROR_INVALID_PARAM; }
    s->buf[s->pos++] = (uint8_t)(val & 0xFF);
    s->buf[s->pos++] = (uint8_t)((val >> 8) & 0xFF);
    s->buf[s->pos++] = (uint8_t)((val >> 16) & 0xFF);
    s->buf[s->pos++] = (uint8_t)((val >> 24) & 0xFF);
    return LGX_SUCCESS;
}

lgx_result_t lgx_net_stream_write_f32(lgx_net_stream_t* s, float val) {
    uint32_t raw;
    memcpy(&raw, &val, sizeof(raw));
    return lgx_net_stream_write_u32(s, raw);
}

lgx_result_t lgx_net_stream_write_string(lgx_net_stream_t* s, const char* str, uint16_t max_len) {
    if (!s || !str) return LGX_ERROR_INVALID_PARAM;

    uint16_t len = 0;
    while (len < max_len - 1 && str[len]) len++;

    lgx_result_t r = lgx_net_stream_write_u16(s, len);
    if (r != LGX_SUCCESS) return r;

    if (s->pos + len > s->capacity) { s->overflow = true; return LGX_ERROR_INVALID_PARAM; }
    memcpy(s->buf + s->pos, str, len);
    s->pos += len;
    return LGX_SUCCESS;
}

/* Read helpers */
uint8_t lgx_net_stream_read_u8(lgx_net_stream_t* s) {
    if (!s || !s->is_read || s->pos + 1 > s->capacity) return 0;
    return s->buf[s->pos++];
}

uint16_t lgx_net_stream_read_u16(lgx_net_stream_t* s) {
    if (!s || !s->is_read || s->pos + 2 > s->capacity) return 0;
    uint16_t val = s->buf[s->pos] | ((uint16_t)s->buf[s->pos + 1] << 8);
    s->pos += 2;
    return val;
}

uint32_t lgx_net_stream_read_u32(lgx_net_stream_t* s) {
    if (!s || !s->is_read || s->pos + 4 > s->capacity) return 0;
    uint32_t val = s->buf[s->pos]
                 | ((uint32_t)s->buf[s->pos + 1] << 8)
                 | ((uint32_t)s->buf[s->pos + 2] << 16)
                 | ((uint32_t)s->buf[s->pos + 3] << 24);
    s->pos += 4;
    return val;
}

float lgx_net_stream_read_f32(lgx_net_stream_t* s) {
    uint32_t raw = lgx_net_stream_read_u32(s);
    float val;
    memcpy(&val, &raw, sizeof(val));
    return val;
}

int lgx_net_stream_read_string(lgx_net_stream_t* s, char* buf, uint16_t max_len) {
    if (!s || !buf || max_len == 0) return 0;

    uint16_t len = lgx_net_stream_read_u16(s);
    if (len == 0) { buf[0] = '\0'; return 0; }

    if (s->pos + len > s->capacity) { buf[0] = '\0'; return 0; }

    uint16_t copy = (len < max_len - 1) ? len : (uint16_t)(max_len - 1);
    memcpy(buf, s->buf + s->pos, copy);
    buf[copy] = '\0';
    s->pos += len;
    return (int)copy;
}

size_t lgx_net_stream_bytes_written(lgx_net_stream_t* s) {
    if (!s) return 0;
    return s->pos;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Delta Compression
 *
 * Format: XOR each byte of prev and curr.
 *   - Non-zero XOR bytes are written directly: [0x01-0xFF] [byte]
 *   - Zero runs are RLE encoded: [0x00] [count_u8]
 *     (count 1-255, multiple records for longer runs)
 * ═══════════════════════════════════════════════════════════════════════════ */

int lgx_net_delta_encode(const void* prev, const void* curr, size_t size,
                         void* out, size_t max_out) {
    if (!prev || !curr || !out || size == 0 || max_out == 0)
        return -1;

    const uint8_t* p = (const uint8_t*)prev;
    const uint8_t* c = (const uint8_t*)curr;
    uint8_t* o = (uint8_t*)out;
    size_t opos = 0;
    size_t i = 0;

    while (i < size) {
        uint8_t xor_val = p[i] ^ c[i];

        if (xor_val == 0) {
            /* Count zero run */
            size_t run_start = i;
            while (i < size && (p[i] ^ c[i]) == 0 && (i - run_start) < 255) {
                i++;
            }
            uint8_t run_len = (uint8_t)(i - run_start);
            if (opos + 2 > max_out) return -1;
            o[opos++] = 0x00;       /* Zero marker */
            o[opos++] = run_len;    /* Run length */
        } else {
            if (opos + 1 > max_out) return -1;
            o[opos++] = xor_val;
            i++;
        }
    }

    return (int)opos;
}

int lgx_net_delta_decode(const void* prev, const void* delta, size_t delta_len,
                         void* out, size_t out_size) {
    if (!prev || !delta || !out || delta_len == 0 || out_size == 0)
        return -1;

    const uint8_t* p = (const uint8_t*)prev;
    const uint8_t* d = (const uint8_t*)delta;
    uint8_t* o = (uint8_t*)out;
    size_t dpos = 0;
    size_t opos = 0;

    while (dpos < delta_len && opos < out_size) {
        uint8_t byte = d[dpos++];

        if (byte == 0x00) {
            /* Zero RLE */
            if (dpos >= delta_len) return -1;  /* Malformed */
            uint8_t run_len = d[dpos++];
            for (uint8_t r = 0; r < run_len && opos < out_size; r++) {
                o[opos] = p[opos];  /* XOR with 0 = copy */
                opos++;
            }
        } else {
            o[opos] = p[opos] ^ byte;
            opos++;
        }
    }

    return (int)opos;
}
