/**
 * LGX Audio Module v1.4 — Public API
 *
 * 3D spatial audio engine: software mixing, distance attenuation,
 * stereo panning, streaming. Built on ALSA for Linux.
 *
 * Design:
 * - Opaque handles (lgx_aud_system_t, lgx_aud_source_t, lgx_aud_buffer_t)
 * - Follows lgx_aud_* naming pattern (LGX_PLATFORM_ARCHITECTURE.md §2.1)
 * - struct_size first field for ABI evolution (§2.5)
 * - Uses lgx_result_t error codes (§2.3)
 *
 * Requires: lgx_runtime (v1.0)
 * System deps: ALSA (libasound)
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under the Apache License, Version 2.0
 */

#ifndef LGX_AUDIO_H
#define LGX_AUDIO_H

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

#define LGX_AUDIO_VERSION_MAJOR  1
#define LGX_AUDIO_VERSION_MINOR  4
#define LGX_AUDIO_VERSION_PATCH  0

/* ═══════════════════════════════════════════════════════════════════════════
 * Opaque Handles
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Audio engine — mixer, output, listener state */
typedef struct lgx_aud_system lgx_aud_system_t;

/** Audio source — a playing/paused/stopped sound instance */
typedef struct lgx_aud_source lgx_aud_source_t;

/** Audio buffer — PCM sample data (shared across sources) */
typedef struct lgx_aud_buffer lgx_aud_buffer_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Enums
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Sample format */
typedef enum lgx_aud_format {
    LGX_AUD_FORMAT_S16  = 0,   /**< Signed 16-bit integer */
    LGX_AUD_FORMAT_F32  = 1    /**< 32-bit float [-1.0, 1.0] */
} lgx_aud_format_t;

/** Source playback state */
typedef enum lgx_aud_state {
    LGX_AUD_STOPPED = 0,
    LGX_AUD_PLAYING = 1,
    LGX_AUD_PAUSED  = 2
} lgx_aud_state_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Audio system configuration */
typedef struct lgx_aud_config {
    size_t   struct_size;       /**< Must be sizeof(lgx_aud_config_t) */
    uint32_t sample_rate;       /**< Output sample rate (default 48000) */
    uint32_t channels;          /**< Output channels (default 2 = stereo) */
    uint32_t buffer_frames;     /**< Frames per buffer period (default 1024) */
    uint32_t max_sources;       /**< Max concurrent sources (default 32) */
    float    master_volume;     /**< Initial master volume [0.0, 1.0] (default 1.0) */
} lgx_aud_config_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * 3D Vector (convenience — avoids math lib dependency)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct lgx_aud_vec3 {
    float x, y, z;
} lgx_aud_vec3_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * System Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create the audio system. Opens ALSA output, initializes mixer.
 *
 * @param config  Configuration (NULL for defaults: 48kHz stereo, 32 sources)
 * @return Opaque audio system handle, or NULL on failure
 */
lgx_aud_system_t* lgx_aud_create(const lgx_aud_config_t* config);

/** Destroy the audio system. Stops all sources, closes output. NULL-safe. */
void lgx_aud_destroy(lgx_aud_system_t* system);

/**
 * Per-frame update: mix all active sources → write to ALSA output.
 * Call once per frame in the game loop.
 */
lgx_result_t lgx_aud_update(lgx_aud_system_t* system);

/** Set master volume [0.0, 1.0]. */
lgx_result_t lgx_aud_set_master_volume(lgx_aud_system_t* system, float volume);

/** Get master volume. */
float lgx_aud_get_master_volume(lgx_aud_system_t* system);

/* ═══════════════════════════════════════════════════════════════════════════
 * Buffer Management
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create an audio buffer from raw PCM data.
 *
 * @param data      PCM sample data (copied internally)
 * @param frames    Number of audio frames
 * @param format    Sample format (S16 or F32)
 * @param rate      Sample rate of the data
 * @param channels  Number of channels (1=mono, 2=stereo)
 */
lgx_aud_buffer_t* lgx_aud_buffer_create(lgx_aud_system_t* system,
                                         const void* data, uint32_t frames,
                                         lgx_aud_format_t format,
                                         uint32_t rate, uint32_t channels);

/**
 * Load audio buffer from a .wav file.
 * Supports PCM 16-bit and 32-bit float, mono/stereo.
 */
lgx_aud_buffer_t* lgx_aud_buffer_create_from_wav(lgx_aud_system_t* system,
                                                   const char* path);

/** Destroy an audio buffer. NULL-safe. */
void lgx_aud_buffer_destroy(lgx_aud_buffer_t* buffer);

/* ═══════════════════════════════════════════════════════════════════════════
 * Source Management
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create an audio source bound to a buffer.
 * Multiple sources can share the same buffer.
 */
lgx_aud_source_t* lgx_aud_source_create(lgx_aud_system_t* system,
                                         lgx_aud_buffer_t* buffer);

/** Destroy a source. NULL-safe. */
void lgx_aud_source_destroy(lgx_aud_source_t* source);

/** Start/resume playback. */
lgx_result_t lgx_aud_source_play(lgx_aud_source_t* source);

/** Stop playback, reset position to beginning. */
lgx_result_t lgx_aud_source_stop(lgx_aud_source_t* source);

/** Pause playback (resume with play). */
lgx_result_t lgx_aud_source_pause(lgx_aud_source_t* source);

/** Get current playback state. */
lgx_aud_state_t lgx_aud_source_get_state(lgx_aud_source_t* source);

/** Enable/disable looping. */
lgx_result_t lgx_aud_source_set_looping(lgx_aud_source_t* source, bool loop);

/** Set per-source volume [0.0, 1.0]. */
lgx_result_t lgx_aud_source_set_volume(lgx_aud_source_t* source, float volume);

/** Set playback pitch/speed (1.0 = normal, 2.0 = double speed). */
lgx_result_t lgx_aud_source_set_pitch(lgx_aud_source_t* source, float pitch);

/* ═══════════════════════════════════════════════════════════════════════════
 * 3D Spatial Audio
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Set source 3D position in world space. */
lgx_result_t lgx_aud_source_set_position(lgx_aud_source_t* source,
                                          float x, float y, float z);

/**
 * Set distance attenuation model for a source.
 *
 * @param ref_distance   Distance at which volume is 100% (default 1.0)
 * @param max_distance   Beyond this, volume is 0 (default 100.0)
 * @param rolloff        Attenuation curve steepness (default 1.0)
 */
lgx_result_t lgx_aud_source_set_distance_model(lgx_aud_source_t* source,
                                                float ref_distance,
                                                float max_distance,
                                                float rolloff);

/** Set listener position in world space. */
lgx_result_t lgx_aud_listener_set_position(lgx_aud_system_t* system,
                                            float x, float y, float z);

/**
 * Set listener orientation.
 *
 * @param fx,fy,fz  Forward direction vector
 * @param ux,uy,uz  Up direction vector
 */
lgx_result_t lgx_aud_listener_set_orientation(lgx_aud_system_t* system,
                                               float fx, float fy, float fz,
                                               float ux, float uy, float uz);

#ifdef __cplusplus
}
#endif

#endif /* LGX_AUDIO_H */
