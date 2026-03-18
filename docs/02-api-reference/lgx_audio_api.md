# LGX Audio Module v1.4 — API Reference

> 3D spatial audio engine with software mixing, ALSA output.

**Requires:** `lgx_runtime` (v1.0), ALSA (`libasound`)
**Header:** `#include "lgx_audio.h"`
**Library:** `-llgx_audio -lasound -lm`

---

## System Lifecycle

| Function | Description |
|----------|-------------|
| `lgx_aud_create(config)` | Create audio system, open ALSA output. NULL config = 48kHz stereo. |
| `lgx_aud_destroy(system)` | Stop all sources, close output. NULL-safe. |
| `lgx_aud_update(system)` | Mix active sources → ALSA output (call once per frame). |
| `lgx_aud_set_master_volume(system, vol)` | Master volume [0.0, 1.0]. |
| `lgx_aud_get_master_volume(system)` | Get current master volume. |

---

## Buffer Management

| Function | Description |
|----------|-------------|
| `lgx_aud_buffer_create(sys, data, frames, fmt, rate, ch)` | Upload raw PCM (S16 or F32, mono/stereo). |
| `lgx_aud_buffer_create_from_wav(sys, path)` | Load from .wav file (PCM 16-bit / 32-bit float). |
| `lgx_aud_buffer_destroy(buffer)` | Free buffer. NULL-safe. |

---

## Source Playback

| Function | Description |
|----------|-------------|
| `lgx_aud_source_create(system, buffer)` | Create source bound to buffer. Multiple sources share buffers. |
| `lgx_aud_source_destroy(source)` | Destroy source. NULL-safe. |
| `lgx_aud_source_play(source)` | Start/resume playback. |
| `lgx_aud_source_stop(source)` | Stop and reset to beginning. |
| `lgx_aud_source_pause(source)` | Pause (resume with play). |
| `lgx_aud_source_get_state(source)` | Returns `STOPPED`, `PLAYING`, or `PAUSED`. |
| `lgx_aud_source_set_looping(source, loop)` | Enable/disable loop. |
| `lgx_aud_source_set_volume(source, vol)` | Per-source volume [0.0, 1.0]. |
| `lgx_aud_source_set_pitch(source, pitch)` | Playback speed (0.1–4.0, 1.0 = normal). |

---

## 3D Spatial Audio

| Function | Description |
|----------|-------------|
| `lgx_aud_source_set_position(src, x, y, z)` | Source position in world space. |
| `lgx_aud_source_set_distance_model(src, ref, max, rolloff)` | Inverse-distance attenuation params. |
| `lgx_aud_listener_set_position(sys, x, y, z)` | Listener position in world space. |
| `lgx_aud_listener_set_orientation(sys, fx,fy,fz, ux,uy,uz)` | Forward + Up vectors. |

**Distance model:** `gain = ref / (ref + rolloff × (distance - ref))`, clamped at max_distance.
**Stereo panning:** Equal-power panning from angle between source and listener right vector.

---

## Features

- **Backend:** ALSA (`snd_pcm_writei`, float LE format)
- **Mixer:** Software mixing of up to 32 sources per frame
- **3D Audio:** Inverse-distance attenuation + equal-power stereo panning
- **WAV Loader:** Built-in RIFF parser (PCM S16, IEEE F32, mono/stereo)
- **Graceful degradation:** Works headless when ALSA unavailable (CI/testing)
