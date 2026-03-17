# LGX Input Module v1.3 — API Reference

> Unified input abstraction: gamepad, keyboard, mouse via Linux evdev.

**Requires:** `lgx_runtime` (v1.0)
**Header:** `#include "lgx_input.h"`
**Library:** `-llgx_input`

---

## System Lifecycle

| Function | Description |
|----------|-------------|
| `lgx_in_create(config)` | Create input system, enumerate `/dev/input/event*`. NULL config = all enabled. |
| `lgx_in_destroy(system)` | Close all devices, free resources. NULL-safe. |
| `lgx_in_poll(system)` | Poll all devices (call once per frame). Updates state, fills event queue. |

---

## Event Queue

| Function | Description |
|----------|-------------|
| `lgx_in_next_event(system, event)` | Dequeue next event. Returns `false` when empty. |

**Event types:** `KEY_PRESS` · `KEY_RELEASE` · `MOUSE_MOVE` · `MOUSE_BUTTON_DOWN/UP` · `MOUSE_SCROLL` · `GAMEPAD_CONNECT/DISCONNECT` · `GAMEPAD_BUTTON_DOWN/UP` · `GAMEPAD_AXIS`

---

## Keyboard

| Function | Description |
|----------|-------------|
| `lgx_in_key_down(system, key)` | Is key currently held? |
| `lgx_in_key_pressed(system, key)` | Was key pressed this frame? (edge-triggered) |
| `lgx_in_key_released(system, key)` | Was key released this frame? (edge-triggered) |

**Keys:** A–Z, 0–9, F1–F12, modifiers (L/R Shift/Ctrl/Alt/Super), arrows, navigation, punctuation (~100 keys)

---

## Mouse

| Function | Description |
|----------|-------------|
| `lgx_in_mouse_get_state(system, state)` | Get position, deltas, scroll, buttons. |
| `lgx_in_mouse_set_relative(system, enable)` | Toggle relative (FPS) mode. |

**Buttons:** `LEFT` · `RIGHT` · `MIDDLE` · `SIDE1` · `SIDE2` (bitmask)

---

## Gamepad

| Function | Description |
|----------|-------------|
| `lgx_in_gamepad_count(system)` | Number of connected gamepads (0–8). |
| `lgx_in_gamepad_connected(system, id)` | Is gamepad `id` connected? |
| `lgx_in_gamepad_get_state(system, id, state)` | Get axes + buttons snapshot. |
| `lgx_in_gamepad_get_name(system, id)` | Device name string. NULL if disconnected. |
| `lgx_in_gamepad_rumble(system, id, low, high, ms)` | Force feedback (FF_RUMBLE). |
| `lgx_in_gamepad_set_deadzone(system, id, dz)` | Set stick deadzone [0.0–1.0], default 0.15. |

**Buttons** (bitmask): `A` · `B` · `X` · `Y` · `LB` · `RB` · `LSTICK` · `RSTICK` · `START` · `SELECT` · `GUIDE` · `DPAD_UP/DOWN/LEFT/RIGHT`

**Axes** (float): `LEFT_X/Y` · `RIGHT_X/Y` [-1,1] · `TRIGGER_L/R` [0,1]

---

## Features

- **Backend:** Direct Linux evdev (`/dev/input/event*`) — no libinput dependency
- **Hot-plug:** `inotify` watch on `/dev/input/` for connect/disconnect
- **Gamepad mapping:** Auto-maps Xbox, PlayStation, generic controllers via `BTN_*`/`ABS_*`
- **Deadzone:** Configurable per-gamepad with smooth rescaling
- **Max devices:** 32 input devices, 8 gamepads
