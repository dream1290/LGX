/**
 * LGX Audio Module v1.4 — Core Implementation
 * Software mixer with 3D spatial audio, ALSA output.
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under Apache License 2.0
 */

#define _GNU_SOURCE
#include "lgx_audio.h"
#include "lgx_runtime.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <alsa/asoundlib.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants & Defaults
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DEFAULT_SAMPLE_RATE   48000
#define DEFAULT_CHANNELS      2
#define DEFAULT_BUFFER_FRAMES 1024
#define DEFAULT_MAX_SOURCES   32

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Audio Buffer
 * ═══════════════════════════════════════════════════════════════════════════ */

struct lgx_aud_buffer {
    float*   samples;       /* Always stored as interleaved float */
    uint32_t total_frames;
    uint32_t channels;
    uint32_t sample_rate;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Audio Source
 * ═══════════════════════════════════════════════════════════════════════════ */

struct lgx_aud_source {
    lgx_aud_buffer_t*  buffer;
    lgx_aud_state_t    state;
    double             position;     /* Playback position in frames (fractional for pitch) */
    float              volume;
    float              pitch;
    bool               looping;

    /* 3D spatial */
    lgx_aud_vec3_t     pos;
    float              ref_distance;
    float              max_distance;
    float              rolloff;

    /* Linked list in system */
    lgx_aud_source_t*  next;
    lgx_aud_system_t*  system;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Audio System
 * ═══════════════════════════════════════════════════════════════════════════ */

struct lgx_aud_system {
    /* ALSA */
    snd_pcm_t*         pcm;
    bool               alsa_open;

    /* Config */
    uint32_t           sample_rate;
    uint32_t           channels;
    uint32_t           buffer_frames;
    uint32_t           max_sources;

    /* Mixer */
    float              master_volume;
    float*             mix_buffer;      /* Interleaved stereo output */

    /* Sources (linked list) */
    lgx_aud_source_t*  sources;
    uint32_t           source_count;

    /* Listener */
    lgx_aud_vec3_t     listener_pos;
    lgx_aud_vec3_t     listener_fwd;
    lgx_aud_vec3_t     listener_up;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Math Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static float vec3_length(lgx_aud_vec3_t a, lgx_aud_vec3_t b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Compute distance attenuation */
static float compute_attenuation(float distance, float ref, float max_dist, float rolloff) {
    if (distance <= ref) return 1.0f;
    if (distance >= max_dist) return 0.0f;
    return ref / (ref + rolloff * (distance - ref));
}

/* Compute stereo panning from 3D positions */
static void compute_panning(lgx_aud_system_t* sys, lgx_aud_vec3_t src_pos,
                             float* left_gain, float* right_gain) {
    float dx = src_pos.x - sys->listener_pos.x;
    float dy = src_pos.y - sys->listener_pos.y;
    float dz = src_pos.z - sys->listener_pos.z;
    float dist = sqrtf(dx*dx + dy*dy + dz*dz);

    if (dist < 0.001f) {
        *left_gain = 1.0f;
        *right_gain = 1.0f;
        return;
    }

    /* Normalize direction */
    float nx = dx / dist, ny = dy / dist, nz = dz / dist;

    /* Right vector = forward × up */
    float rx = sys->listener_fwd.y * sys->listener_up.z - sys->listener_fwd.z * sys->listener_up.y;
    float ry = sys->listener_fwd.z * sys->listener_up.x - sys->listener_fwd.x * sys->listener_up.z;
    float rz = sys->listener_fwd.x * sys->listener_up.y - sys->listener_fwd.y * sys->listener_up.x;
    float rlen = sqrtf(rx*rx + ry*ry + rz*rz);
    if (rlen > 0.001f) { rx /= rlen; ry /= rlen; rz /= rlen; }

    /* Dot product with right vector: -1 = full left, +1 = full right */
    float pan = clampf(nx*rx + ny*ry + nz*rz, -1.0f, 1.0f);

    /* Equal-power panning */
    float angle = (pan + 1.0f) * 0.5f * 1.5707963f;  /* 0 to π/2 */
    *left_gain = cosf(angle);
    *right_gain = sinf(angle);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ALSA Setup
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool setup_alsa(lgx_aud_system_t* sys) {
    int err = snd_pcm_open(&sys->pcm, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (err < 0) {
        printf("[LGX AUDIO] ALSA open failed: %s\n", snd_strerror(err));
        return false;
    }

    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(sys->pcm, params);

    snd_pcm_hw_params_set_access(sys->pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(sys->pcm, params, SND_PCM_FORMAT_FLOAT_LE);
    snd_pcm_hw_params_set_channels(sys->pcm, params, sys->channels);

    unsigned int rate = sys->sample_rate;
    snd_pcm_hw_params_set_rate_near(sys->pcm, params, &rate, NULL);
    sys->sample_rate = rate;

    snd_pcm_uframes_t buffer_size = sys->buffer_frames * 4;
    snd_pcm_hw_params_set_buffer_size_near(sys->pcm, params, &buffer_size);

    snd_pcm_uframes_t period_size = sys->buffer_frames;
    snd_pcm_hw_params_set_period_size_near(sys->pcm, params, &period_size, NULL);

    err = snd_pcm_hw_params(sys->pcm, params);
    if (err < 0) {
        printf("[LGX AUDIO] ALSA hw_params failed: %s\n", snd_strerror(err));
        snd_pcm_close(sys->pcm);
        return false;
    }

    snd_pcm_prepare(sys->pcm);
    sys->alsa_open = true;

    printf("[LGX AUDIO] ALSA output: %u Hz, %u ch, %lu frame periods\n",
           sys->sample_rate, sys->channels, (unsigned long)period_size);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * System Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_aud_system_t* lgx_aud_create(const lgx_aud_config_t* config) {
    lgx_aud_system_t* sys = calloc(1, sizeof(*sys));
    if (!sys) return NULL;

    sys->sample_rate   = (config && config->sample_rate > 0) ? config->sample_rate : DEFAULT_SAMPLE_RATE;
    sys->channels      = (config && config->channels > 0) ? config->channels : DEFAULT_CHANNELS;
    sys->buffer_frames = (config && config->buffer_frames > 0) ? config->buffer_frames : DEFAULT_BUFFER_FRAMES;
    sys->max_sources   = (config && config->max_sources > 0) ? config->max_sources : DEFAULT_MAX_SOURCES;
    sys->master_volume = (config && config->master_volume > 0.0f) ? config->master_volume : 1.0f;

    /* Default listener: at origin, facing -Z, up +Y */
    sys->listener_fwd = (lgx_aud_vec3_t){0.0f, 0.0f, -1.0f};
    sys->listener_up  = (lgx_aud_vec3_t){0.0f, 1.0f, 0.0f};

    /* Mix buffer */
    sys->mix_buffer = calloc(sys->buffer_frames * sys->channels, sizeof(float));
    if (!sys->mix_buffer) { free(sys); return NULL; }

    /* ALSA output */
    if (!setup_alsa(sys)) {
        printf("[LGX AUDIO] Warning: no ALSA output (tests still work)\n");
        /* Continue without ALSA — allows headless testing */
    }

    printf("[LGX AUDIO] System initialized: %u Hz, %u ch, max %u sources\n",
           sys->sample_rate, sys->channels, sys->max_sources);
    return sys;
}

void lgx_aud_destroy(lgx_aud_system_t* sys) {
    if (!sys) return;

    /* Destroy all sources */
    lgx_aud_source_t* src = sys->sources;
    while (src) {
        lgx_aud_source_t* next = src->next;
        free(src);
        src = next;
    }

    if (sys->alsa_open && sys->pcm) {
        snd_pcm_drain(sys->pcm);
        snd_pcm_close(sys->pcm);
    }

    free(sys->mix_buffer);
    free(sys);
    printf("[LGX AUDIO] System destroyed\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Mixer / Update
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_result_t lgx_aud_update(lgx_aud_system_t* sys) {
    if (!sys) return LGX_ERROR_INVALID_PARAM;

    uint32_t frames = sys->buffer_frames;
    uint32_t ch = sys->channels;
    float* mix = sys->mix_buffer;

    /* Clear mix buffer */
    memset(mix, 0, frames * ch * sizeof(float));

    /* Mix all playing sources */
    lgx_aud_source_t* src = sys->sources;
    while (src) {
        if (src->state != LGX_AUD_PLAYING || !src->buffer) {
            src = src->next;
            continue;
        }

        lgx_aud_buffer_t* buf = src->buffer;
        float vol = src->volume * sys->master_volume;

        /* 3D spatial: distance attenuation + panning */
        float dist = vec3_length(src->pos, sys->listener_pos);
        float atten = compute_attenuation(dist, src->ref_distance,
                                          src->max_distance, src->rolloff);
        float left_gain = 1.0f, right_gain = 1.0f;
        if (ch >= 2) {
            compute_panning(sys, src->pos, &left_gain, &right_gain);
        }

        float final_vol = vol * atten;

        /* Read samples with pitch */
        for (uint32_t f = 0; f < frames; f++) {
            uint32_t src_frame = (uint32_t)src->position;

            if (src_frame >= buf->total_frames) {
                if (src->looping) {
                    src->position = 0.0;
                    src_frame = 0;
                } else {
                    src->state = LGX_AUD_STOPPED;
                    src->position = 0.0;
                    break;
                }
            }

            if (buf->channels == 1) {
                /* Mono source → stereo output */
                float sample = buf->samples[src_frame] * final_vol;
                if (ch >= 2) {
                    mix[f * ch + 0] += sample * left_gain;
                    mix[f * ch + 1] += sample * right_gain;
                } else {
                    mix[f * ch + 0] += sample;
                }
            } else {
                /* Stereo source */
                float sL = buf->samples[src_frame * 2 + 0] * final_vol;
                float sR = buf->samples[src_frame * 2 + 1] * final_vol;
                if (ch >= 2) {
                    mix[f * ch + 0] += sL * left_gain;
                    mix[f * ch + 1] += sR * right_gain;
                }
            }

            src->position += (double)src->pitch;
        }

        src = src->next;
    }

    /* Clamp output */
    for (uint32_t i = 0; i < frames * ch; i++) {
        mix[i] = clampf(mix[i], -1.0f, 1.0f);
    }

    /* Write to ALSA */
    if (sys->alsa_open && sys->pcm) {
        snd_pcm_sframes_t written = snd_pcm_writei(sys->pcm, mix, frames);
        if (written < 0) {
            snd_pcm_recover(sys->pcm, (int)written, 1);
        }
    }

    return LGX_SUCCESS;
}

lgx_result_t lgx_aud_set_master_volume(lgx_aud_system_t* sys, float volume) {
    if (!sys) return LGX_ERROR_INVALID_PARAM;
    sys->master_volume = clampf(volume, 0.0f, 1.0f);
    return LGX_SUCCESS;
}

float lgx_aud_get_master_volume(lgx_aud_system_t* sys) {
    if (!sys) return 0.0f;
    return sys->master_volume;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Buffer Management
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_aud_buffer_t* lgx_aud_buffer_create(lgx_aud_system_t* sys,
                                         const void* data, uint32_t frames,
                                         lgx_aud_format_t format,
                                         uint32_t rate, uint32_t channels) {
    if (!sys || !data || frames == 0 || channels == 0) return NULL;

    lgx_aud_buffer_t* buf = calloc(1, sizeof(*buf));
    if (!buf) return NULL;

    buf->total_frames = frames;
    buf->channels = channels;
    buf->sample_rate = rate;

    /* Convert to float internally */
    uint32_t total_samples = frames * channels;
    buf->samples = malloc(total_samples * sizeof(float));
    if (!buf->samples) { free(buf); return NULL; }

    if (format == LGX_AUD_FORMAT_F32) {
        memcpy(buf->samples, data, total_samples * sizeof(float));
    } else {
        /* S16 → float */
        const int16_t* s16 = (const int16_t*)data;
        for (uint32_t i = 0; i < total_samples; i++) {
            buf->samples[i] = (float)s16[i] / 32768.0f;
        }
    }

    return buf;
}

/* WAV file format structures */
#pragma pack(push, 1)
typedef struct {
    char     riff_id[4];      /* "RIFF" */
    uint32_t file_size;
    char     wave_id[4];      /* "WAVE" */
} wav_header_t;

typedef struct {
    char     chunk_id[4];
    uint32_t chunk_size;
} wav_chunk_t;

typedef struct {
    uint16_t format;          /* 1=PCM, 3=IEEE float */
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} wav_fmt_t;
#pragma pack(pop)

lgx_aud_buffer_t* lgx_aud_buffer_create_from_wav(lgx_aud_system_t* sys,
                                                   const char* path) {
    if (!sys || !path) return NULL;

    FILE* f = fopen(path, "rb");
    if (!f) { printf("[LGX AUDIO] Cannot open: %s\n", path); return NULL; }

    wav_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1 ||
        memcmp(header.riff_id, "RIFF", 4) != 0 ||
        memcmp(header.wave_id, "WAVE", 4) != 0) {
        printf("[LGX AUDIO] Not a WAV file: %s\n", path);
        fclose(f);
        return NULL;
    }

    wav_fmt_t fmt = {0};
    bool found_fmt = false, found_data = false;
    uint32_t data_size = 0;
    void* raw_data = NULL;

    while (!feof(f)) {
        wav_chunk_t chunk;
        if (fread(&chunk, sizeof(chunk), 1, f) != 1) break;

        if (memcmp(chunk.chunk_id, "fmt ", 4) == 0) {
            size_t read_size = chunk.chunk_size < sizeof(fmt) ? chunk.chunk_size : sizeof(fmt);
            if (fread(&fmt, read_size, 1, f) != 1) break;
            if (chunk.chunk_size > read_size)
                fseek(f, (long)(chunk.chunk_size - read_size), SEEK_CUR);
            found_fmt = true;
        } else if (memcmp(chunk.chunk_id, "data", 4) == 0) {
            data_size = chunk.chunk_size;
            raw_data = malloc(data_size);
            if (!raw_data || fread(raw_data, data_size, 1, f) != 1) {
                free(raw_data);
                fclose(f);
                return NULL;
            }
            found_data = true;
            break;
        } else {
            fseek(f, (long)chunk.chunk_size, SEEK_CUR);
        }
    }
    fclose(f);

    if (!found_fmt || !found_data || !raw_data) {
        free(raw_data);
        return NULL;
    }

    /* Determine format */
    lgx_aud_format_t aud_fmt;
    if (fmt.format == 1 && fmt.bits_per_sample == 16)
        aud_fmt = LGX_AUD_FORMAT_S16;
    else if (fmt.format == 3 && fmt.bits_per_sample == 32)
        aud_fmt = LGX_AUD_FORMAT_F32;
    else {
        printf("[LGX AUDIO] Unsupported WAV format: fmt=%u bits=%u\n",
               fmt.format, fmt.bits_per_sample);
        free(raw_data);
        return NULL;
    }

    uint32_t bytes_per_sample = fmt.bits_per_sample / 8;
    uint32_t total_samples = data_size / bytes_per_sample;
    uint32_t frames = total_samples / fmt.channels;

    lgx_aud_buffer_t* buf = lgx_aud_buffer_create(sys, raw_data, frames,
                                                    aud_fmt, fmt.sample_rate,
                                                    fmt.channels);
    free(raw_data);

    if (buf) {
        printf("[LGX AUDIO] Loaded WAV: %s (%u frames, %u Hz, %u ch)\n",
               path, frames, fmt.sample_rate, fmt.channels);
    }
    return buf;
}

void lgx_aud_buffer_destroy(lgx_aud_buffer_t* buf) {
    if (!buf) return;
    free(buf->samples);
    free(buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Source Management
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_aud_source_t* lgx_aud_source_create(lgx_aud_system_t* sys,
                                         lgx_aud_buffer_t* buffer) {
    if (!sys || !buffer) return NULL;
    if (sys->source_count >= sys->max_sources) return NULL;

    lgx_aud_source_t* src = calloc(1, sizeof(*src));
    if (!src) return NULL;

    src->buffer = buffer;
    src->state = LGX_AUD_STOPPED;
    src->volume = 1.0f;
    src->pitch = 1.0f;
    src->ref_distance = 1.0f;
    src->max_distance = 100.0f;
    src->rolloff = 1.0f;
    src->system = sys;

    /* Prepend to linked list */
    src->next = sys->sources;
    sys->sources = src;
    sys->source_count++;

    return src;
}

void lgx_aud_source_destroy(lgx_aud_source_t* src) {
    if (!src || !src->system) return;

    lgx_aud_system_t* sys = src->system;

    /* Remove from linked list */
    lgx_aud_source_t** pp = &sys->sources;
    while (*pp) {
        if (*pp == src) {
            *pp = src->next;
            sys->source_count--;
            break;
        }
        pp = &(*pp)->next;
    }

    free(src);
}

lgx_result_t lgx_aud_source_play(lgx_aud_source_t* src) {
    if (!src) return LGX_ERROR_INVALID_PARAM;
    src->state = LGX_AUD_PLAYING;
    return LGX_SUCCESS;
}

lgx_result_t lgx_aud_source_stop(lgx_aud_source_t* src) {
    if (!src) return LGX_ERROR_INVALID_PARAM;
    src->state = LGX_AUD_STOPPED;
    src->position = 0.0;
    return LGX_SUCCESS;
}

lgx_result_t lgx_aud_source_pause(lgx_aud_source_t* src) {
    if (!src) return LGX_ERROR_INVALID_PARAM;
    if (src->state == LGX_AUD_PLAYING) src->state = LGX_AUD_PAUSED;
    return LGX_SUCCESS;
}

lgx_aud_state_t lgx_aud_source_get_state(lgx_aud_source_t* src) {
    if (!src) return LGX_AUD_STOPPED;
    return src->state;
}

lgx_result_t lgx_aud_source_set_looping(lgx_aud_source_t* src, bool loop) {
    if (!src) return LGX_ERROR_INVALID_PARAM;
    src->looping = loop;
    return LGX_SUCCESS;
}

lgx_result_t lgx_aud_source_set_volume(lgx_aud_source_t* src, float volume) {
    if (!src) return LGX_ERROR_INVALID_PARAM;
    src->volume = clampf(volume, 0.0f, 1.0f);
    return LGX_SUCCESS;
}

lgx_result_t lgx_aud_source_set_pitch(lgx_aud_source_t* src, float pitch) {
    if (!src) return LGX_ERROR_INVALID_PARAM;
    src->pitch = clampf(pitch, 0.1f, 4.0f);
    return LGX_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3D Spatial Audio
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_result_t lgx_aud_source_set_position(lgx_aud_source_t* src,
                                          float x, float y, float z) {
    if (!src) return LGX_ERROR_INVALID_PARAM;
    src->pos = (lgx_aud_vec3_t){x, y, z};
    return LGX_SUCCESS;
}

lgx_result_t lgx_aud_source_set_distance_model(lgx_aud_source_t* src,
                                                float ref_distance,
                                                float max_distance,
                                                float rolloff) {
    if (!src) return LGX_ERROR_INVALID_PARAM;
    if (ref_distance < 0.0f || max_distance < ref_distance || rolloff < 0.0f)
        return LGX_ERROR_INVALID_PARAM;
    src->ref_distance = ref_distance;
    src->max_distance = max_distance;
    src->rolloff = rolloff;
    return LGX_SUCCESS;
}

lgx_result_t lgx_aud_listener_set_position(lgx_aud_system_t* sys,
                                            float x, float y, float z) {
    if (!sys) return LGX_ERROR_INVALID_PARAM;
    sys->listener_pos = (lgx_aud_vec3_t){x, y, z};
    return LGX_SUCCESS;
}

lgx_result_t lgx_aud_listener_set_orientation(lgx_aud_system_t* sys,
                                               float fx, float fy, float fz,
                                               float ux, float uy, float uz) {
    if (!sys) return LGX_ERROR_INVALID_PARAM;
    sys->listener_fwd = (lgx_aud_vec3_t){fx, fy, fz};
    sys->listener_up  = (lgx_aud_vec3_t){ux, uy, uz};
    return LGX_SUCCESS;
}
