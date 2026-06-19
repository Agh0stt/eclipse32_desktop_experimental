// =============================================================================
// Eclipse32 - Keyboard Driver Header
// =============================================================================
#pragma once
#include "../../kernel.h"

typedef enum {
    KEY_UNKNOWN = 0,
    KEY_ASCII,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_END, KEY_PGUP, KEY_PGDN,
    KEY_INSERT, KEY_DELETE,
    KEY_F1,  KEY_F2,  KEY_F3,  KEY_F4,
    KEY_F5,  KEY_F6,  KEY_F7,  KEY_F8,
    KEY_F9,  KEY_F10, KEY_F11, KEY_F12,
    KEY_ESC, KEY_TAB, KEY_BACKSPACE, KEY_ENTER,
} key_code_t;

typedef struct {
    key_code_t keycode;
    char       ascii;
    bool       released;
    bool       shift;
    bool       ctrl;
    bool       alt;
    bool       caps;
} key_event_t;

void        keyboard_init(void);
bool        keyboard_has_event(void);
key_event_t keyboard_get_event(void);
key_event_t keyboard_wait_event(void);
char        keyboard_getchar(void);
