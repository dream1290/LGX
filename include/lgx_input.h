/**
 * LGX Input Module v1.3 — Public API
 *
 * Unified input abstraction: gamepad, keyboard, mouse, touch.
 * Built on Linux evdev for maximum hardware compatibility.
 *
 * Design:
 * - Opaque handles (lgx_in_system_t)
 * - Follows lgx_in_* naming pattern (LGX_PLATFORM_ARCHITECTURE.md §2.1)
 * - struct_size first field for ABI evolution (§2.5)
 * - Uses lgx_result_t error codes (§2.3)
 * - Thread-safe: poll from one thread, query from any
 *
 * Requires: lgx_runtime (v1.0)
 * Optional: lgx_threading (v1.1) for background event pump
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under the Apache License, Version 2.0
 */

#ifndef LGX_INPUT_H
#define LGX_INPUT_H

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

#define LGX_INPUT_VERSION_MAJOR  1
#define LGX_INPUT_VERSION_MINOR  3
#define LGX_INPUT_VERSION_PATCH  0

/* ═══════════════════════════════════════════════════════════════════════════
 * Opaque Handle
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Input system — manages all devices, event queue, hot-plug detection */
typedef struct lgx_in_system lgx_in_system_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Keyboard Keys (subset matching USB HID)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum lgx_in_key {
    /* Letters */
    LGX_KEY_A = 0, LGX_KEY_B, LGX_KEY_C, LGX_KEY_D, LGX_KEY_E,
    LGX_KEY_F, LGX_KEY_G, LGX_KEY_H, LGX_KEY_I, LGX_KEY_J,
    LGX_KEY_K, LGX_KEY_L, LGX_KEY_M, LGX_KEY_N, LGX_KEY_O,
    LGX_KEY_P, LGX_KEY_Q, LGX_KEY_R, LGX_KEY_S, LGX_KEY_T,
    LGX_KEY_U, LGX_KEY_V, LGX_KEY_W, LGX_KEY_X, LGX_KEY_Y,
    LGX_KEY_Z,

    /* Numbers */
    LGX_KEY_0, LGX_KEY_1, LGX_KEY_2, LGX_KEY_3, LGX_KEY_4,
    LGX_KEY_5, LGX_KEY_6, LGX_KEY_7, LGX_KEY_8, LGX_KEY_9,

    /* Function keys */
    LGX_KEY_F1, LGX_KEY_F2, LGX_KEY_F3, LGX_KEY_F4,
    LGX_KEY_F5, LGX_KEY_F6, LGX_KEY_F7, LGX_KEY_F8,
    LGX_KEY_F9, LGX_KEY_F10, LGX_KEY_F11, LGX_KEY_F12,

    /* Modifiers */
    LGX_KEY_LSHIFT, LGX_KEY_RSHIFT,
    LGX_KEY_LCTRL, LGX_KEY_RCTRL,
    LGX_KEY_LALT, LGX_KEY_RALT,
    LGX_KEY_LSUPER, LGX_KEY_RSUPER,

    /* Navigation */
    LGX_KEY_UP, LGX_KEY_DOWN, LGX_KEY_LEFT, LGX_KEY_RIGHT,
    LGX_KEY_HOME, LGX_KEY_END, LGX_KEY_PAGEUP, LGX_KEY_PAGEDOWN,
    LGX_KEY_INSERT, LGX_KEY_DELETE,

    /* Common */
    LGX_KEY_ESCAPE, LGX_KEY_ENTER, LGX_KEY_TAB, LGX_KEY_BACKSPACE,
    LGX_KEY_SPACE, LGX_KEY_CAPSLOCK, LGX_KEY_NUMLOCK, LGX_KEY_SCROLLLOCK,
    LGX_KEY_PRINTSCREEN, LGX_KEY_PAUSE,

    /* Punctuation */
    LGX_KEY_MINUS, LGX_KEY_EQUALS, LGX_KEY_LBRACKET, LGX_KEY_RBRACKET,
    LGX_KEY_BACKSLASH, LGX_KEY_SEMICOLON, LGX_KEY_APOSTROPHE,
    LGX_KEY_GRAVE, LGX_KEY_COMMA, LGX_KEY_PERIOD, LGX_KEY_SLASH,

    LGX_KEY_COUNT  /**< Total key count (for bitfield sizing) */
} lgx_in_key_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Mouse Buttons
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum lgx_in_mouse_button {
    LGX_MOUSE_LEFT   = 0x01,
    LGX_MOUSE_RIGHT  = 0x02,
    LGX_MOUSE_MIDDLE = 0x04,
    LGX_MOUSE_SIDE1  = 0x08,
    LGX_MOUSE_SIDE2  = 0x10
} lgx_in_mouse_button_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Gamepad
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Gamepad buttons (bitmask) */
typedef enum lgx_in_gamepad_button {
    LGX_PAD_A       = 0x0001,  /**< Xbox A / PS Cross */
    LGX_PAD_B       = 0x0002,  /**< Xbox B / PS Circle */
    LGX_PAD_X       = 0x0004,  /**< Xbox X / PS Square */
    LGX_PAD_Y       = 0x0008,  /**< Xbox Y / PS Triangle */
    LGX_PAD_LB      = 0x0010,  /**< Left bumper / L1 */
    LGX_PAD_RB      = 0x0020,  /**< Right bumper / R1 */
    LGX_PAD_LSTICK  = 0x0040,  /**< Left stick click / L3 */
    LGX_PAD_RSTICK  = 0x0080,  /**< Right stick click / R3 */
    LGX_PAD_START   = 0x0100,  /**< Start / Options */
    LGX_PAD_SELECT  = 0x0200,  /**< Select / Share */
    LGX_PAD_GUIDE   = 0x0400,  /**< Guide / PS button */
    LGX_PAD_DPAD_UP    = 0x0800,
    LGX_PAD_DPAD_DOWN  = 0x1000,
    LGX_PAD_DPAD_LEFT  = 0x2000,
    LGX_PAD_DPAD_RIGHT = 0x4000
} lgx_in_gamepad_button_t;

/** Gamepad axes */
typedef enum lgx_in_gamepad_axis {
    LGX_PAD_AXIS_LEFT_X   = 0,
    LGX_PAD_AXIS_LEFT_Y   = 1,
    LGX_PAD_AXIS_RIGHT_X  = 2,
    LGX_PAD_AXIS_RIGHT_Y  = 3,
    LGX_PAD_AXIS_TRIGGER_L = 4,
    LGX_PAD_AXIS_TRIGGER_R = 5,
    LGX_PAD_AXIS_COUNT     = 6
} lgx_in_gamepad_axis_t;

#define LGX_IN_MAX_GAMEPADS 8

/* ═══════════════════════════════════════════════════════════════════════════
 * Event Types
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum lgx_in_event_type {
    LGX_EVENT_NONE              = 0,
    LGX_EVENT_KEY_PRESS         = 1,
    LGX_EVENT_KEY_RELEASE       = 2,
    LGX_EVENT_MOUSE_MOVE        = 3,
    LGX_EVENT_MOUSE_BUTTON_DOWN = 4,
    LGX_EVENT_MOUSE_BUTTON_UP   = 5,
    LGX_EVENT_MOUSE_SCROLL      = 6,
    LGX_EVENT_GAMEPAD_CONNECT   = 7,
    LGX_EVENT_GAMEPAD_DISCONNECT = 8,
    LGX_EVENT_GAMEPAD_BUTTON_DOWN = 9,
    LGX_EVENT_GAMEPAD_BUTTON_UP   = 10,
    LGX_EVENT_GAMEPAD_AXIS        = 11
} lgx_in_event_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Configuration & State Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Input system configuration */
typedef struct lgx_in_config {
    size_t struct_size;             /**< Must be sizeof(lgx_in_config_t) */
    bool enable_gamepad;            /**< Enable gamepad detection (default true) */
    bool enable_keyboard;           /**< Enable keyboard input (default true) */
    bool enable_mouse;              /**< Enable mouse input (default true) */
    uint32_t event_queue_size;      /**< Max queued events (default 256) */
} lgx_in_config_t;

/** Gamepad state snapshot */
typedef struct lgx_in_gamepad_state {
    size_t struct_size;             /**< Must be sizeof(lgx_in_gamepad_state_t) */
    uint32_t buttons;               /**< Bitmask of lgx_in_gamepad_button_t */
    float axes[LGX_PAD_AXIS_COUNT]; /**< Axis values, range [-1.0, 1.0] (triggers: [0.0, 1.0]) */
    bool connected;                 /**< true if gamepad is plugged in */
    char name[128];                 /**< Device name string */
} lgx_in_gamepad_state_t;

/** Mouse state snapshot */
typedef struct lgx_in_mouse_state {
    size_t struct_size;             /**< Must be sizeof(lgx_in_mouse_state_t) */
    int32_t x;                      /**< Absolute X position */
    int32_t y;                      /**< Absolute Y position */
    int32_t delta_x;                /**< X movement since last poll */
    int32_t delta_y;                /**< Y movement since last poll */
    int32_t scroll;                 /**< Scroll wheel delta since last poll */
    uint32_t buttons;               /**< Bitmask of lgx_in_mouse_button_t */
} lgx_in_mouse_state_t;

/** Input event (from event queue) */
typedef struct lgx_in_event {
    lgx_in_event_type_t type;       /**< Event type */
    uint64_t timestamp_ns;          /**< Event timestamp (nanoseconds) */
    union {
        struct { lgx_in_key_t key; }            key;        /**< KEY_PRESS / KEY_RELEASE */
        struct { int32_t x, y, dx, dy; }        mouse_move; /**< MOUSE_MOVE */
        struct { lgx_in_mouse_button_t button; } mouse_btn;  /**< MOUSE_BUTTON_* */
        struct { int32_t delta; }               scroll;     /**< MOUSE_SCROLL */
        struct { uint32_t pad_id; }             gamepad_connect; /**< CONNECT/DISCONNECT */
        struct { uint32_t pad_id; lgx_in_gamepad_button_t button; } gamepad_btn; /**< BUTTON_* */
        struct { uint32_t pad_id; lgx_in_gamepad_axis_t axis; float value; } gamepad_axis; /**< AXIS */
    } data;
} lgx_in_event_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * System Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create the input system. Enumerates /dev/input/event* devices,
 * sets up inotify for hot-plug detection.
 *
 * @param config  Configuration (NULL for defaults: all inputs enabled)
 * @return Opaque input system handle, or NULL on failure
 */
lgx_in_system_t* lgx_in_create(const lgx_in_config_t* config);

/** Destroy the input system. Closes all device fds. NULL-safe. */
void lgx_in_destroy(lgx_in_system_t* system);

/**
 * Poll all input devices. Call once per frame.
 * Updates keyboard, mouse, and gamepad state. Fills event queue.
 */
lgx_result_t lgx_in_poll(lgx_in_system_t* system);

/* ═══════════════════════════════════════════════════════════════════════════
 * Event Queue
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Dequeue the next input event.
 * Returns true if an event was returned, false if queue is empty.
 */
bool lgx_in_next_event(lgx_in_system_t* system, lgx_in_event_t* event);

/* ═══════════════════════════════════════════════════════════════════════════
 * Keyboard API
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Is key currently held down? */
bool lgx_in_key_down(lgx_in_system_t* system, lgx_in_key_t key);

/** Was key pressed this frame? (edge-triggered) */
bool lgx_in_key_pressed(lgx_in_system_t* system, lgx_in_key_t key);

/** Was key released this frame? (edge-triggered) */
bool lgx_in_key_released(lgx_in_system_t* system, lgx_in_key_t key);

/* ═══════════════════════════════════════════════════════════════════════════
 * Mouse API
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Get current mouse state (position, deltas, buttons). */
lgx_result_t lgx_in_mouse_get_state(lgx_in_system_t* system,
                                     lgx_in_mouse_state_t* state);

/** Enable/disable relative (FPS) mouse mode. */
lgx_result_t lgx_in_mouse_set_relative(lgx_in_system_t* system, bool enable);

/* ═══════════════════════════════════════════════════════════════════════════
 * Gamepad API
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Get number of currently connected gamepads (0..LGX_IN_MAX_GAMEPADS). */
uint32_t lgx_in_gamepad_count(lgx_in_system_t* system);

/** Is gamepad `pad_id` connected? */
bool lgx_in_gamepad_connected(lgx_in_system_t* system, uint32_t pad_id);

/** Get full gamepad state (axes + buttons). */
lgx_result_t lgx_in_gamepad_get_state(lgx_in_system_t* system, uint32_t pad_id,
                                       lgx_in_gamepad_state_t* state);

/** Get gamepad name string. Returns NULL if not connected. */
const char* lgx_in_gamepad_get_name(lgx_in_system_t* system, uint32_t pad_id);

/**
 * Trigger rumble/force feedback.
 *
 * @param low_freq   Low-frequency motor intensity [0.0, 1.0]
 * @param high_freq  High-frequency motor intensity [0.0, 1.0]
 * @param duration_ms Duration in milliseconds
 */
lgx_result_t lgx_in_gamepad_rumble(lgx_in_system_t* system, uint32_t pad_id,
                                    float low_freq, float high_freq,
                                    uint32_t duration_ms);

/**
 * Set analog stick deadzone.
 *
 * @param deadzone  Deadzone radius [0.0, 1.0] (default 0.15)
 */
lgx_result_t lgx_in_gamepad_set_deadzone(lgx_in_system_t* system,
                                          uint32_t pad_id, float deadzone);

#ifdef __cplusplus
}
#endif

#endif /* LGX_INPUT_H */
