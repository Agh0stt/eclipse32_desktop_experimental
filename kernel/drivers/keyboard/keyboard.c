// =============================================================================
// Eclipse32 - PS/2 Keyboard Driver
// Full US scancode set 1, key event queue, special key support
// =============================================================================
#include "keyboard.h"
#include "../../arch/x86/idt.h"
#include "../../arch/x86/pic.h"
#include "../../kernel.h"

// Key event ring buffer
#define KEY_BUF_SIZE    256

static key_event_t key_buf[KEY_BUF_SIZE];
static uint32_t    key_head = 0;
static uint32_t    key_tail = 0;

// Modifier state
static bool shift_held  = false;
static bool ctrl_held   = false;
static bool alt_held    = false;
static bool caps_lock   = false;
static bool extended    = false;    // E0 prefix received

// ---- Scancode Set 1 -> ASCII tables ----
// Normal (no shift)
static const char sc_normal[128] = {
    0,    27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-',  '=',  '\b', '\t',
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',  '\n', 0,
    'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',  0,   '\\',
    'z',  'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',  0,   '*',  0,   ' ',
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,    0,   0,
    0,    0,   0,   0,   0,   0,   '-', 0,   0,   0,    '+', 0,    0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,
};

// Shifted
static const char sc_shifted[128] = {
    0,    27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_',  '+',  '\b', '\t',
    'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',  '\n', 0,
    'A',  'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"',  '~',  0,   '|',
    'Z',  'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',  0,   '*',  0,   ' ',
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,    0,   0,
    0,    0,   0,   0,   0,   0,   '-', 0,   0,   0,    '+', 0,    0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,
};

// Special scancode -> keycode mappings (when E0 prefix)
static key_code_t extended_keys[128];

static void init_extended_map(void) {
    for (int i = 0; i < 128; i++) extended_keys[i] = KEY_UNKNOWN;
    extended_keys[0x48] = KEY_UP;
    extended_keys[0x50] = KEY_DOWN;
    extended_keys[0x4B] = KEY_LEFT;
    extended_keys[0x4D] = KEY_RIGHT;
    extended_keys[0x47] = KEY_HOME;
    extended_keys[0x4F] = KEY_END;
    extended_keys[0x49] = KEY_PGUP;
    extended_keys[0x51] = KEY_PGDN;
    extended_keys[0x52] = KEY_INSERT;
    extended_keys[0x53] = KEY_DELETE;
}

// Function key map (F1-F12 at scancodes 0x3B-0x44, 0x57, 0x58)
static key_code_t fkey_map[12] = {
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,  KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12
};

static void keyboard_handler(void *regs) {
    (void)regs;

    uint8_t sc = inb(0x60);

    // Extended key prefix
    if (sc == 0xE0) { extended = true; return; }

    bool key_up = (sc & 0x80) != 0;
    sc &= 0x7F;

    key_event_t event = {0};
    event.released  = key_up;
    event.shift     = shift_held;
    event.ctrl      = ctrl_held;
    event.alt       = alt_held;
    event.caps      = caps_lock;

    if (extended) {
        extended = false;
        event.keycode = extended_keys[sc < 128 ? sc : 0];
        event.ascii   = 0;

        // Extended modifiers
        if (sc == 0x1D) { ctrl_held = !key_up; return; }
        if (sc == 0x38) { alt_held  = !key_up; return; }
    } else {
        // Regular keys
        switch (sc) {
        case 0x2A: case 0x36: shift_held = !key_up; return;
        case 0x1D: ctrl_held  = !key_up; return;
        case 0x38: alt_held   = !key_up; return;
        case 0x3A:
            if (!key_up) caps_lock = !caps_lock;
            return;
        default: break;
        }

        // Function keys
        if (sc >= 0x3B && sc <= 0x44) {
            event.keycode = fkey_map[sc - 0x3B];
        } else if (sc == 0x57) {
            event.keycode = KEY_F11;
        } else if (sc == 0x58) {
            event.keycode = KEY_F12;
        } else {
            event.keycode = KEY_ASCII;
            bool use_shift = shift_held ^ caps_lock;
            char c = use_shift ? sc_shifted[sc] : sc_normal[sc];

            // Ctrl modifier
            if (ctrl_held && c >= 'a' && c <= 'z') c -= 96;  // ctrl+a = 0x01
            if (ctrl_held && c >= 'A' && c <= 'Z') c -= 64;

            event.ascii = c;
        }
    }

    // Only queue key-down events (or all events if needed)
    if (!key_up || event.keycode != KEY_ASCII) {
        uint32_t next = (key_head + 1) % KEY_BUF_SIZE;
        if (next != key_tail) {
            key_buf[key_head] = event;
            key_head = next;
        }
    }
}

void keyboard_init(void) {
    init_extended_map();
    key_head = key_tail = 0;

    // Flush any pending data
    while (inb(0x64) & 1) inb(0x60);

    idt_register_handler(33, keyboard_handler);  // IRQ1 = INT 33
    pic_unmask_irq(1);
}

bool keyboard_has_event(void) {
    return key_head != key_tail;
}

key_event_t keyboard_get_event(void) {
    if (key_head == key_tail) {
        key_event_t empty = {0};
        return empty;
    }
    key_event_t ev = key_buf[key_tail];
    key_tail = (key_tail + 1) % KEY_BUF_SIZE;
    return ev;
}

// Blocking read of a single ASCII character
char keyboard_getchar(void) {
    while (1) {
        asm volatile("hlt");
        if (!keyboard_has_event()) continue;
        key_event_t ev = keyboard_get_event();
        if (ev.released) continue;
        if (ev.keycode == KEY_ASCII && ev.ascii) return ev.ascii;
    }
}

// Wait for key event (blocking)
key_event_t keyboard_wait_event(void) {
    while (!keyboard_has_event()) {
        asm volatile("hlt");
    }
    return keyboard_get_event();
}
