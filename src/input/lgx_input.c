/**
 * LGX Input Module v1.3 — Core Implementation
 * Unified input via Linux evdev: keyboard, mouse, gamepad, hot-plug.
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under Apache License 2.0
 */

#define _GNU_SOURCE
#include "lgx_input.h"
#include "lgx_runtime.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>

#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <sys/select.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

#define MAX_DEVICES        32
#define DEFAULT_QUEUE_SIZE 256
#define DEFAULT_DEADZONE   0.15f
#define INPUT_DIR          "/dev/input"

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal Device Type
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    DEV_NONE     = 0,
    DEV_KEYBOARD = 1,
    DEV_MOUSE    = 2,
    DEV_GAMEPAD  = 3
} device_type_t;

typedef struct {
    int              fd;
    device_type_t    type;
    char             name[128];
    char             path[64];
    uint32_t         gamepad_id;      /* Index into gamepad state array */
    int              ff_id;           /* Force feedback effect id (-1 = none) */
} input_device_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal Gamepad State
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    bool             connected;
    char             name[128];
    float            axes[LGX_PAD_AXIS_COUNT];
    uint32_t         buttons;
    float            deadzone;
    int              abs_min[ABS_MAX + 1];
    int              abs_max[ABS_MAX + 1];
    input_device_t*  device;
} gamepad_internal_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal System State
 * ═══════════════════════════════════════════════════════════════════════════ */

struct lgx_in_system {
    /* Devices */
    input_device_t   devices[MAX_DEVICES];
    uint32_t         device_count;

    /* inotify for hot-plug */
    int              inotify_fd;
    int              inotify_wd;

    /* Keyboard state */
    uint8_t          key_current[LGX_KEY_COUNT];   /* Current frame: 1 = down */
    uint8_t          key_previous[LGX_KEY_COUNT];  /* Previous frame state */

    /* Mouse state */
    lgx_in_mouse_state_t mouse;
    bool             mouse_relative;

    /* Gamepad state */
    gamepad_internal_t gamepads[LGX_IN_MAX_GAMEPADS];
    uint32_t         gamepad_count;

    /* Event queue (ring buffer) */
    lgx_in_event_t*  events;
    uint32_t         event_capacity;
    uint32_t         event_head;
    uint32_t         event_tail;
    uint32_t         event_count;

    /* Config */
    bool             enable_gamepad;
    bool             enable_keyboard;
    bool             enable_mouse;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Event Queue Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static void event_push(lgx_in_system_t* sys, const lgx_in_event_t* evt) {
    if (sys->event_count >= sys->event_capacity) return;  /* Drop oldest if full */
    sys->events[sys->event_tail] = *evt;
    sys->event_tail = (sys->event_tail + 1) % sys->event_capacity;
    sys->event_count++;
}

static uint64_t time_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * evdev Key Code → LGX Key Translation
 * ═══════════════════════════════════════════════════════════════════════════ */

static lgx_in_key_t evdev_to_lgx_key(uint16_t code) {
    switch (code) {
        case KEY_A: return LGX_KEY_A;  case KEY_B: return LGX_KEY_B;
        case KEY_C: return LGX_KEY_C;  case KEY_D: return LGX_KEY_D;
        case KEY_E: return LGX_KEY_E;  case KEY_F: return LGX_KEY_F;
        case KEY_G: return LGX_KEY_G;  case KEY_H: return LGX_KEY_H;
        case KEY_I: return LGX_KEY_I;  case KEY_J: return LGX_KEY_J;
        case KEY_K: return LGX_KEY_K;  case KEY_L: return LGX_KEY_L;
        case KEY_M: return LGX_KEY_M;  case KEY_N: return LGX_KEY_N;
        case KEY_O: return LGX_KEY_O;  case KEY_P: return LGX_KEY_P;
        case KEY_Q: return LGX_KEY_Q;  case KEY_R: return LGX_KEY_R;
        case KEY_S: return LGX_KEY_S;  case KEY_T: return LGX_KEY_T;
        case KEY_U: return LGX_KEY_U;  case KEY_V: return LGX_KEY_V;
        case KEY_W: return LGX_KEY_W;  case KEY_X: return LGX_KEY_X;
        case KEY_Y: return LGX_KEY_Y;  case KEY_Z: return LGX_KEY_Z;

        case KEY_0: return LGX_KEY_0;  case KEY_1: return LGX_KEY_1;
        case KEY_2: return LGX_KEY_2;  case KEY_3: return LGX_KEY_3;
        case KEY_4: return LGX_KEY_4;  case KEY_5: return LGX_KEY_5;
        case KEY_6: return LGX_KEY_6;  case KEY_7: return LGX_KEY_7;
        case KEY_8: return LGX_KEY_8;  case KEY_9: return LGX_KEY_9;

        case KEY_F1: return LGX_KEY_F1;   case KEY_F2: return LGX_KEY_F2;
        case KEY_F3: return LGX_KEY_F3;   case KEY_F4: return LGX_KEY_F4;
        case KEY_F5: return LGX_KEY_F5;   case KEY_F6: return LGX_KEY_F6;
        case KEY_F7: return LGX_KEY_F7;   case KEY_F8: return LGX_KEY_F8;
        case KEY_F9: return LGX_KEY_F9;   case KEY_F10: return LGX_KEY_F10;
        case KEY_F11: return LGX_KEY_F11; case KEY_F12: return LGX_KEY_F12;

        case KEY_LEFTSHIFT: return LGX_KEY_LSHIFT;
        case KEY_RIGHTSHIFT: return LGX_KEY_RSHIFT;
        case KEY_LEFTCTRL: return LGX_KEY_LCTRL;
        case KEY_RIGHTCTRL: return LGX_KEY_RCTRL;
        case KEY_LEFTALT: return LGX_KEY_LALT;
        case KEY_RIGHTALT: return LGX_KEY_RALT;
        case KEY_LEFTMETA: return LGX_KEY_LSUPER;
        case KEY_RIGHTMETA: return LGX_KEY_RSUPER;

        case KEY_UP: return LGX_KEY_UP;       case KEY_DOWN: return LGX_KEY_DOWN;
        case KEY_LEFT: return LGX_KEY_LEFT;   case KEY_RIGHT: return LGX_KEY_RIGHT;
        case KEY_HOME: return LGX_KEY_HOME;   case KEY_END: return LGX_KEY_END;
        case KEY_PAGEUP: return LGX_KEY_PAGEUP;
        case KEY_PAGEDOWN: return LGX_KEY_PAGEDOWN;
        case KEY_INSERT: return LGX_KEY_INSERT;
        case KEY_DELETE: return LGX_KEY_DELETE;

        case KEY_ESC: return LGX_KEY_ESCAPE;
        case KEY_ENTER: return LGX_KEY_ENTER;
        case KEY_TAB: return LGX_KEY_TAB;
        case KEY_BACKSPACE: return LGX_KEY_BACKSPACE;
        case KEY_SPACE: return LGX_KEY_SPACE;
        case KEY_CAPSLOCK: return LGX_KEY_CAPSLOCK;
        case KEY_NUMLOCK: return LGX_KEY_NUMLOCK;
        case KEY_SCROLLLOCK: return LGX_KEY_SCROLLLOCK;
        case KEY_SYSRQ: return LGX_KEY_PRINTSCREEN;
        case KEY_PAUSE: return LGX_KEY_PAUSE;

        case KEY_MINUS: return LGX_KEY_MINUS;
        case KEY_EQUAL: return LGX_KEY_EQUALS;
        case KEY_LEFTBRACE: return LGX_KEY_LBRACKET;
        case KEY_RIGHTBRACE: return LGX_KEY_RBRACKET;
        case KEY_BACKSLASH: return LGX_KEY_BACKSLASH;
        case KEY_SEMICOLON: return LGX_KEY_SEMICOLON;
        case KEY_APOSTROPHE: return LGX_KEY_APOSTROPHE;
        case KEY_GRAVE: return LGX_KEY_GRAVE;
        case KEY_COMMA: return LGX_KEY_COMMA;
        case KEY_DOT: return LGX_KEY_PERIOD;
        case KEY_SLASH: return LGX_KEY_SLASH;

        default: return LGX_KEY_COUNT;  /* Unknown */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Gamepad Button Mapping (evdev → LGX)
 * ═══════════════════════════════════════════════════════════════════════════ */

static lgx_in_gamepad_button_t evdev_to_gamepad_btn(uint16_t code) {
    switch (code) {
        case BTN_SOUTH:  return LGX_PAD_A;
        case BTN_EAST:   return LGX_PAD_B;
        case BTN_NORTH:  return LGX_PAD_Y;
        case BTN_WEST:   return LGX_PAD_X;
        case BTN_TL:     return LGX_PAD_LB;
        case BTN_TR:     return LGX_PAD_RB;
        case BTN_THUMBL: return LGX_PAD_LSTICK;
        case BTN_THUMBR: return LGX_PAD_RSTICK;
        case BTN_START:  return LGX_PAD_START;
        case BTN_SELECT: return LGX_PAD_SELECT;
        case BTN_MODE:   return LGX_PAD_GUIDE;
        default:         return 0;
    }
}

/* Normalize axis value from evdev range to [-1, 1] or [0, 1] for triggers */
static float normalize_axis(int value, int min, int max) {
    if (max == min) return 0.0f;
    return 2.0f * (float)(value - min) / (float)(max - min) - 1.0f;
}

static float apply_deadzone(float value, float deadzone) {
    if (value > -deadzone && value < deadzone) return 0.0f;
    float sign = (value > 0.0f) ? 1.0f : -1.0f;
    float adj = (value - sign * deadzone) / (1.0f - deadzone);
    return (adj > 1.0f) ? 1.0f : (adj < -1.0f) ? -1.0f : adj;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Device Detection & Classification
 * ═══════════════════════════════════════════════════════════════════════════ */

static device_type_t classify_device(int fd) {
    unsigned long evbits[((EV_MAX + 1) + 63) / 64] = {0};
    unsigned long keybits[((KEY_MAX + 1) + 63) / 64] = {0};
    unsigned long absbits[((ABS_MAX + 1) + 63) / 64] = {0};

    ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), evbits);

    #define TEST_BIT(arr, bit) ((arr[(bit) / 64] >> ((bit) % 64)) & 1UL)

    bool has_keys = TEST_BIT(evbits, EV_KEY);
    bool has_abs  = TEST_BIT(evbits, EV_ABS);
    bool has_rel  = TEST_BIT(evbits, EV_REL);

    if (has_keys) ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits);
    if (has_abs)  ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits);

    /* Gamepad: has BTN_GAMEPAD or BTN_SOUTH + ABS_X */
    if (has_keys && has_abs) {
        bool has_gamepad_btn = TEST_BIT(keybits, BTN_GAMEPAD) || TEST_BIT(keybits, BTN_SOUTH);
        bool has_stick = TEST_BIT(absbits, ABS_X) && TEST_BIT(absbits, ABS_Y);
        if (has_gamepad_btn && has_stick) return DEV_GAMEPAD;
    }

    /* Mouse: has REL_X + BTN_LEFT */
    if (has_rel && has_keys && TEST_BIT(keybits, BTN_LEFT)) return DEV_MOUSE;

    /* Keyboard: has many letter keys */
    if (has_keys && TEST_BIT(keybits, KEY_A) && TEST_BIT(keybits, KEY_Z) &&
        TEST_BIT(keybits, KEY_SPACE)) return DEV_KEYBOARD;

    #undef TEST_BIT
    return DEV_NONE;
}

static void open_device(lgx_in_system_t* sys, const char* path) {
    if (sys->device_count >= MAX_DEVICES) return;

    /* Check if already open */
    for (uint32_t i = 0; i < sys->device_count; i++) {
        if (strcmp(sys->devices[i].path, path) == 0) return;
    }

    int fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) return;
    }

    device_type_t type = classify_device(fd);
    if (type == DEV_NONE ||
        (type == DEV_KEYBOARD && !sys->enable_keyboard) ||
        (type == DEV_MOUSE && !sys->enable_mouse) ||
        (type == DEV_GAMEPAD && !sys->enable_gamepad)) {
        close(fd);
        return;
    }

    input_device_t* dev = &sys->devices[sys->device_count];
    memset(dev, 0, sizeof(*dev));
    dev->fd = fd;
    dev->type = type;
    dev->ff_id = -1;
    strncpy(dev->path, path, sizeof(dev->path) - 1);

    char name[128] = "Unknown";
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    strncpy(dev->name, name, sizeof(dev->name) - 1);

    /* Gamepad setup */
    if (type == DEV_GAMEPAD && sys->gamepad_count < LGX_IN_MAX_GAMEPADS) {
        uint32_t gid = sys->gamepad_count++;
        dev->gamepad_id = gid;
        gamepad_internal_t* gp = &sys->gamepads[gid];
        memset(gp, 0, sizeof(*gp));
        gp->connected = true;
        gp->deadzone = DEFAULT_DEADZONE;
        gp->device = dev;
        strncpy(gp->name, name, sizeof(gp->name) - 1);

        /* Read axis ranges */
        uint16_t axes[] = { ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ,
                            ABS_HAT0X, ABS_HAT0Y };
        for (size_t i = 0; i < sizeof(axes)/sizeof(axes[0]); i++) {
            struct input_absinfo info;
            if (ioctl(fd, EVIOCGABS(axes[i]), &info) == 0) {
                gp->abs_min[axes[i]] = info.minimum;
                gp->abs_max[axes[i]] = info.maximum;
            }
        }

        /* Push connect event */
        lgx_in_event_t evt = {
            .type = LGX_EVENT_GAMEPAD_CONNECT,
            .timestamp_ns = time_now_ns(),
            .data.gamepad_connect.pad_id = gid,
        };
        event_push(sys, &evt);
        printf("[LGX INPUT] Gamepad %u: %s\n", gid, name);
    }

    sys->device_count++;
    printf("[LGX INPUT] Device: %s (%s)\n", name,
           type == DEV_KEYBOARD ? "keyboard" :
           type == DEV_MOUSE ? "mouse" : "gamepad");
}

static void enumerate_devices(lgx_in_system_t* sys) {
    DIR* dir = opendir(INPUT_DIR);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;
        char path[280];
        snprintf(path, sizeof(path), "%s/%s", INPUT_DIR, entry->d_name);
        open_device(sys, path);
    }
    closedir(dir);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Event Processing
 * ═══════════════════════════════════════════════════════════════════════════ */

static void process_keyboard_event(lgx_in_system_t* sys, const struct input_event* ev) {
    if (ev->type != EV_KEY) return;
    lgx_in_key_t key = evdev_to_lgx_key(ev->code);
    if (key >= LGX_KEY_COUNT) return;

    if (ev->value == 1) {  /* Press */
        sys->key_current[key] = 1;
        lgx_in_event_t evt = {
            .type = LGX_EVENT_KEY_PRESS,
            .timestamp_ns = time_now_ns(),
            .data.key.key = key,
        };
        event_push(sys, &evt);
    } else if (ev->value == 0) {  /* Release */
        sys->key_current[key] = 0;
        lgx_in_event_t evt = {
            .type = LGX_EVENT_KEY_RELEASE,
            .timestamp_ns = time_now_ns(),
            .data.key.key = key,
        };
        event_push(sys, &evt);
    }
    /* value == 2 → repeat, ignored */
}

static void process_mouse_event(lgx_in_system_t* sys, const struct input_event* ev) {
    if (ev->type == EV_REL) {
        if (ev->code == REL_X) {
            sys->mouse.delta_x += ev->value;
            if (!sys->mouse_relative) sys->mouse.x += ev->value;
        } else if (ev->code == REL_Y) {
            sys->mouse.delta_y += ev->value;
            if (!sys->mouse_relative) sys->mouse.y += ev->value;
        } else if (ev->code == REL_WHEEL) {
            sys->mouse.scroll += ev->value;
            lgx_in_event_t evt = {
                .type = LGX_EVENT_MOUSE_SCROLL,
                .timestamp_ns = time_now_ns(),
                .data.scroll.delta = ev->value,
            };
            event_push(sys, &evt);
        }
    } else if (ev->type == EV_KEY) {
        uint32_t btn_mask = 0;
        switch (ev->code) {
            case BTN_LEFT:   btn_mask = LGX_MOUSE_LEFT; break;
            case BTN_RIGHT:  btn_mask = LGX_MOUSE_RIGHT; break;
            case BTN_MIDDLE: btn_mask = LGX_MOUSE_MIDDLE; break;
            case BTN_SIDE:   btn_mask = LGX_MOUSE_SIDE1; break;
            case BTN_EXTRA:  btn_mask = LGX_MOUSE_SIDE2; break;
            default: return;
        }
        if (ev->value) sys->mouse.buttons |= btn_mask;
        else           sys->mouse.buttons &= ~btn_mask;

        lgx_in_event_t evt = {
            .type = ev->value ? LGX_EVENT_MOUSE_BUTTON_DOWN : LGX_EVENT_MOUSE_BUTTON_UP,
            .timestamp_ns = time_now_ns(),
            .data.mouse_btn.button = (lgx_in_mouse_button_t)btn_mask,
        };
        event_push(sys, &evt);
    }
}

static void process_gamepad_event(lgx_in_system_t* sys, input_device_t* dev,
                                   const struct input_event* ev) {
    if (dev->gamepad_id >= LGX_IN_MAX_GAMEPADS) return;
    gamepad_internal_t* gp = &sys->gamepads[dev->gamepad_id];

    if (ev->type == EV_KEY) {
        lgx_in_gamepad_button_t btn = evdev_to_gamepad_btn(ev->code);
        if (btn == 0) return;

        if (ev->value) gp->buttons |= btn;
        else           gp->buttons &= ~btn;

        lgx_in_event_t evt = {
            .type = ev->value ? LGX_EVENT_GAMEPAD_BUTTON_DOWN : LGX_EVENT_GAMEPAD_BUTTON_UP,
            .timestamp_ns = time_now_ns(),
            .data.gamepad_btn = { .pad_id = dev->gamepad_id, .button = btn },
        };
        event_push(sys, &evt);
    } else if (ev->type == EV_ABS) {
        lgx_in_gamepad_axis_t axis;
        float value;

        switch (ev->code) {
            case ABS_X:     axis = LGX_PAD_AXIS_LEFT_X; break;
            case ABS_Y:     axis = LGX_PAD_AXIS_LEFT_Y; break;
            case ABS_RX:    axis = LGX_PAD_AXIS_RIGHT_X; break;
            case ABS_RY:    axis = LGX_PAD_AXIS_RIGHT_Y; break;
            case ABS_Z:     axis = LGX_PAD_AXIS_TRIGGER_L; break;
            case ABS_RZ:    axis = LGX_PAD_AXIS_TRIGGER_R; break;
            case ABS_HAT0X:
                /* D-pad X: -1 = left, +1 = right */
                if (ev->value < 0) { gp->buttons |= LGX_PAD_DPAD_LEFT; gp->buttons &= ~LGX_PAD_DPAD_RIGHT; }
                else if (ev->value > 0) { gp->buttons |= LGX_PAD_DPAD_RIGHT; gp->buttons &= ~LGX_PAD_DPAD_LEFT; }
                else { gp->buttons &= ~(LGX_PAD_DPAD_LEFT | LGX_PAD_DPAD_RIGHT); }
                return;
            case ABS_HAT0Y:
                /* D-pad Y: -1 = up, +1 = down */
                if (ev->value < 0) { gp->buttons |= LGX_PAD_DPAD_UP; gp->buttons &= ~LGX_PAD_DPAD_DOWN; }
                else if (ev->value > 0) { gp->buttons |= LGX_PAD_DPAD_DOWN; gp->buttons &= ~LGX_PAD_DPAD_UP; }
                else { gp->buttons &= ~(LGX_PAD_DPAD_UP | LGX_PAD_DPAD_DOWN); }
                return;
            default: return;
        }

        value = normalize_axis(ev->value, gp->abs_min[ev->code], gp->abs_max[ev->code]);

        /* Triggers: remap from [-1,1] to [0,1] */
        if (axis == LGX_PAD_AXIS_TRIGGER_L || axis == LGX_PAD_AXIS_TRIGGER_R) {
            value = (value + 1.0f) * 0.5f;
        } else {
            value = apply_deadzone(value, gp->deadzone);
        }

        gp->axes[axis] = value;

        lgx_in_event_t evt = {
            .type = LGX_EVENT_GAMEPAD_AXIS,
            .timestamp_ns = time_now_ns(),
            .data.gamepad_axis = { .pad_id = dev->gamepad_id, .axis = axis, .value = value },
        };
        event_push(sys, &evt);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Hot-Plug (inotify)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void check_hotplug(lgx_in_system_t* sys) {
    if (sys->inotify_fd < 0) return;

    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    ssize_t len = read(sys->inotify_fd, buf, sizeof(buf));
    if (len <= 0) return;

    for (char* ptr = buf; ptr < buf + len; ) {
        struct inotify_event* ev = (struct inotify_event*)ptr;
        if (ev->len > 0 && strncmp(ev->name, "event", 5) == 0) {
            char path[280];
            snprintf(path, sizeof(path), "%s/%s", INPUT_DIR, ev->name);

            if (ev->mask & IN_CREATE) {
                /* Small delay for udev to finish setup */
                usleep(100000);
                open_device(sys, path);
            }
            /* IN_DELETE handled by read() failing on the fd */
        }
        ptr += sizeof(struct inotify_event) + ev->len;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * System Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_in_system_t* lgx_in_create(const lgx_in_config_t* config) {
    lgx_in_system_t* sys = calloc(1, sizeof(*sys));
    if (!sys) return NULL;

    /* Defaults */
    sys->enable_gamepad  = config ? config->enable_gamepad  : true;
    sys->enable_keyboard = config ? config->enable_keyboard : true;
    sys->enable_mouse    = config ? config->enable_mouse    : true;

    uint32_t queue_size = (config && config->event_queue_size > 0)
                          ? config->event_queue_size : DEFAULT_QUEUE_SIZE;
    sys->events = calloc(queue_size, sizeof(lgx_in_event_t));
    if (!sys->events) { free(sys); return NULL; }
    sys->event_capacity = queue_size;

    sys->mouse.struct_size = sizeof(lgx_in_mouse_state_t);

    /* inotify for hot-plug */
    sys->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (sys->inotify_fd >= 0) {
        sys->inotify_wd = inotify_add_watch(sys->inotify_fd, INPUT_DIR,
                                             IN_CREATE | IN_DELETE);
    }

    /* Enumerate existing devices */
    enumerate_devices(sys);

    printf("[LGX INPUT] System initialized: %u device(s), %u gamepad(s)\n",
           sys->device_count, sys->gamepad_count);
    return sys;
}

void lgx_in_destroy(lgx_in_system_t* sys) {
    if (!sys) return;

    for (uint32_t i = 0; i < sys->device_count; i++) {
        if (sys->devices[i].fd >= 0) close(sys->devices[i].fd);
    }
    if (sys->inotify_fd >= 0) close(sys->inotify_fd);

    free(sys->events);
    free(sys);
    printf("[LGX INPUT] System destroyed\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Poll
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_result_t lgx_in_poll(lgx_in_system_t* sys) {
    if (!sys) return LGX_ERROR_INVALID_PARAM;

    /* Save previous keyboard state for edge detection */
    memcpy(sys->key_previous, sys->key_current, sizeof(sys->key_current));

    /* Reset per-frame mouse deltas */
    sys->mouse.delta_x = 0;
    sys->mouse.delta_y = 0;
    sys->mouse.scroll = 0;

    /* Check for hot-plug events */
    check_hotplug(sys);

    /* Read events from all devices */
    struct input_event evbuf[64];
    for (uint32_t i = 0; i < sys->device_count; i++) {
        input_device_t* dev = &sys->devices[i];
        if (dev->fd < 0) continue;

        for (;;) {
            ssize_t n = read(dev->fd, evbuf, sizeof(evbuf));
            if (n <= 0) break;

            size_t count = (size_t)n / sizeof(struct input_event);
            for (size_t j = 0; j < count; j++) {
                const struct input_event* ev = &evbuf[j];
                if (ev->type == EV_SYN) continue;

                switch (dev->type) {
                    case DEV_KEYBOARD: process_keyboard_event(sys, ev); break;
                    case DEV_MOUSE:    process_mouse_event(sys, ev); break;
                    case DEV_GAMEPAD:  process_gamepad_event(sys, dev, ev); break;
                    default: break;
                }
            }
        }

        /* Check for disconnection */
        if (errno == ENODEV && dev->type == DEV_GAMEPAD) {
            uint32_t gid = dev->gamepad_id;
            if (gid < LGX_IN_MAX_GAMEPADS) {
                sys->gamepads[gid].connected = false;
                lgx_in_event_t evt = {
                    .type = LGX_EVENT_GAMEPAD_DISCONNECT,
                    .timestamp_ns = time_now_ns(),
                    .data.gamepad_connect.pad_id = gid,
                };
                event_push(sys, &evt);
                printf("[LGX INPUT] Gamepad %u disconnected\n", gid);
            }
            close(dev->fd);
            dev->fd = -1;
        }
    }

    return LGX_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Event Queue
 * ═══════════════════════════════════════════════════════════════════════════ */

bool lgx_in_next_event(lgx_in_system_t* sys, lgx_in_event_t* event) {
    if (!sys || !event || sys->event_count == 0) return false;
    *event = sys->events[sys->event_head];
    sys->event_head = (sys->event_head + 1) % sys->event_capacity;
    sys->event_count--;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Keyboard API
 * ═══════════════════════════════════════════════════════════════════════════ */

bool lgx_in_key_down(lgx_in_system_t* sys, lgx_in_key_t key) {
    if (!sys || key >= LGX_KEY_COUNT) return false;
    return sys->key_current[key] != 0;
}

bool lgx_in_key_pressed(lgx_in_system_t* sys, lgx_in_key_t key) {
    if (!sys || key >= LGX_KEY_COUNT) return false;
    return sys->key_current[key] && !sys->key_previous[key];
}

bool lgx_in_key_released(lgx_in_system_t* sys, lgx_in_key_t key) {
    if (!sys || key >= LGX_KEY_COUNT) return false;
    return !sys->key_current[key] && sys->key_previous[key];
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Mouse API
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_result_t lgx_in_mouse_get_state(lgx_in_system_t* sys,
                                     lgx_in_mouse_state_t* state) {
    if (!sys || !state) return LGX_ERROR_INVALID_PARAM;
    memcpy(state, &sys->mouse, sizeof(lgx_in_mouse_state_t));
    return LGX_SUCCESS;
}

lgx_result_t lgx_in_mouse_set_relative(lgx_in_system_t* sys, bool enable) {
    if (!sys) return LGX_ERROR_INVALID_PARAM;
    sys->mouse_relative = enable;
    if (enable) { sys->mouse.x = 0; sys->mouse.y = 0; }
    return LGX_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Gamepad API
 * ═══════════════════════════════════════════════════════════════════════════ */

uint32_t lgx_in_gamepad_count(lgx_in_system_t* sys) {
    if (!sys) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < sys->gamepad_count; i++) {
        if (sys->gamepads[i].connected) count++;
    }
    return count;
}

bool lgx_in_gamepad_connected(lgx_in_system_t* sys, uint32_t pad_id) {
    if (!sys || pad_id >= LGX_IN_MAX_GAMEPADS) return false;
    return pad_id < sys->gamepad_count && sys->gamepads[pad_id].connected;
}

lgx_result_t lgx_in_gamepad_get_state(lgx_in_system_t* sys, uint32_t pad_id,
                                       lgx_in_gamepad_state_t* state) {
    if (!sys || !state || pad_id >= LGX_IN_MAX_GAMEPADS) return LGX_ERROR_INVALID_PARAM;
    if (pad_id >= sys->gamepad_count || !sys->gamepads[pad_id].connected)
        return LGX_ERROR_INVALID_PARAM;

    gamepad_internal_t* gp = &sys->gamepads[pad_id];
    state->struct_size = sizeof(lgx_in_gamepad_state_t);
    state->buttons = gp->buttons;
    memcpy(state->axes, gp->axes, sizeof(state->axes));
    state->connected = gp->connected;
    strncpy(state->name, gp->name, sizeof(state->name) - 1);
    return LGX_SUCCESS;
}

const char* lgx_in_gamepad_get_name(lgx_in_system_t* sys, uint32_t pad_id) {
    if (!sys || pad_id >= sys->gamepad_count) return NULL;
    if (!sys->gamepads[pad_id].connected) return NULL;
    return sys->gamepads[pad_id].name;
}

lgx_result_t lgx_in_gamepad_rumble(lgx_in_system_t* sys, uint32_t pad_id,
                                    float low_freq, float high_freq,
                                    uint32_t duration_ms) {
    if (!sys || pad_id >= sys->gamepad_count) return LGX_ERROR_INVALID_PARAM;
    gamepad_internal_t* gp = &sys->gamepads[pad_id];
    if (!gp->connected || !gp->device || gp->device->fd < 0)
        return LGX_ERROR_INVALID_PARAM;

    struct ff_effect effect = {
        .type = FF_RUMBLE,
        .id = gp->device->ff_id,
        .u.rumble = {
            .strong_magnitude = (uint16_t)(low_freq * 65535.0f),
            .weak_magnitude = (uint16_t)(high_freq * 65535.0f),
        },
        .replay.length = (uint16_t)(duration_ms > 65535 ? 65535 : duration_ms),
        .replay.delay = 0,
    };

    if (ioctl(gp->device->fd, EVIOCSFF, &effect) < 0) return LGX_ERROR_INVALID_PARAM;
    gp->device->ff_id = effect.id;

    struct input_event play = {
        .type = EV_FF,
        .code = (uint16_t)effect.id,
        .value = 1,
    };
    if (write(gp->device->fd, &play, sizeof(play)) < 0) return LGX_ERROR_INVALID_PARAM;

    return LGX_SUCCESS;
}

lgx_result_t lgx_in_gamepad_set_deadzone(lgx_in_system_t* sys,
                                          uint32_t pad_id, float deadzone) {
    if (!sys || pad_id >= sys->gamepad_count) return LGX_ERROR_INVALID_PARAM;
    if (deadzone < 0.0f || deadzone > 1.0f) return LGX_ERROR_INVALID_PARAM;
    sys->gamepads[pad_id].deadzone = deadzone;
    return LGX_SUCCESS;
}
