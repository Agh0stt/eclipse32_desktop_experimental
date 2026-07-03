// =============================================================================
// Eclipse32 - PS/2 Mouse Driver
// Handles IRQ12 mouse packets, maintains cursor position, event queue
// =============================================================================
#include "mouse.h"
#include "../../arch/x86/idt.h"
#include "../../arch/x86/pic.h"
#include "../../kernel.h"

#define MOUSE_DATA_PORT     0x60
#define MOUSE_STATUS_PORT   0x64
#define MOUSE_CMD_PORT      0x64

#define MOUSE_IRQ           12
#define MOUSE_QUEUE_SIZE    64

// Screen bounds (set by GUI init)
static int32_t g_screen_w = 1024;
static int32_t g_screen_h = 768;

// Current absolute position
static volatile int32_t g_mouse_x = 512;
static volatile int32_t g_mouse_y = 384;
static volatile bool    g_btn_left   = false;
static volatile bool    g_btn_right  = false;
static volatile bool    g_btn_mid    = false;

// 3-byte packet accumulator
static uint8_t  g_packet[3];
static int      g_byte_idx = 0;

// Event ring buffer
static mouse_event_t g_queue[MOUSE_QUEUE_SIZE];
static volatile int  g_q_head = 0;
static volatile int  g_q_tail = 0;

// ---- PS/2 helpers -----------------------------------------------------------

static void mouse_wait_write(void) {
    int timeout = 100000;
    while (timeout-- && (inb(MOUSE_STATUS_PORT) & 0x02));
}

static void mouse_wait_read(void) {
    int timeout = 100000;
    while (timeout-- && !(inb(MOUSE_STATUS_PORT) & 0x01));
}

static void mouse_write(uint8_t data) {
    mouse_wait_write();
    outb(MOUSE_CMD_PORT, 0xD4);   // tell controller: next byte is for mouse
    mouse_wait_write();
    outb(MOUSE_DATA_PORT, data);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(MOUSE_DATA_PORT);
}

// ---- IRQ handler ------------------------------------------------------------

static void mouse_irq_handler(void *regs) {
    (void)regs;

    uint8_t byte = inb(MOUSE_DATA_PORT);

    // Basic sync: byte 0 must have bit 3 set
    if (g_byte_idx == 0 && !(byte & 0x08)) {
        // NOTE: no manual EOI here -- see bottom of function.
        return;
    }

    g_packet[g_byte_idx++] = byte;

    if (g_byte_idx == 3) {
        g_byte_idx = 0;

        uint8_t flags = g_packet[0];
        int32_t dx =  (int32_t)(int8_t)g_packet[1];
        int32_t dy = -(int32_t)(int8_t)g_packet[2];  // Y is inverted in PS/2

        // Overflow bits — discard packet
        if ((flags & 0x40) || (flags & 0x80)) {
            // NOTE: no manual EOI here -- see bottom of function.
            return;
        }

        g_mouse_x += dx;
        g_mouse_y += dy;

        // Clamp to screen
        if (g_mouse_x < 0)              g_mouse_x = 0;
        if (g_mouse_x >= g_screen_w)    g_mouse_x = g_screen_w - 1;
        if (g_mouse_y < 0)              g_mouse_y = 0;
        if (g_mouse_y >= g_screen_h)    g_mouse_y = g_screen_h - 1;

        g_btn_left   = !!(flags & 0x01);
        g_btn_right  = !!(flags & 0x02);
        g_btn_mid    = !!(flags & 0x04);

        // Enqueue event
        int next = (g_q_tail + 1) % MOUSE_QUEUE_SIZE;
        if (next != g_q_head) {
            mouse_event_t ev;
            ev.dx         = dx;
            ev.dy         = dy;
            ev.btn_left   = g_btn_left;
            ev.btn_right  = g_btn_right;
            ev.btn_middle = g_btn_mid;
            g_queue[g_q_tail] = ev;
            g_q_tail = next;
        }
    }

    // NOTE: do not send the PIC EOI here. interrupt_dispatch() in idt.c
    // already sends a single unconditional EOI (both PIC2 and PIC1, since
    // IRQ12 is on the slave) for every hardware IRQ right after this
    // handler returns. The three pic_send_eoi(MOUSE_IRQ) calls that used
    // to be in this function (one per early-return branch, one here) each
    // doubled up with that generic EOI -- and since the mouse moves
    // constantly during normal use, this was a much higher-frequency
    // source of 8259 in-service corruption than the RTL8139 double-EOI
    // (same root cause, fixed the same way, in rtl8139_irq_handler).
}

// ---- Public API -------------------------------------------------------------

void mouse_init(void) {
    // Enable auxiliary PS/2 device
    mouse_wait_write();
    outb(MOUSE_CMD_PORT, 0xA8);

    // Enable interrupt for auxiliary device
    mouse_wait_write();
    outb(MOUSE_CMD_PORT, 0x20);
    mouse_wait_read();
    uint8_t status = inb(MOUSE_DATA_PORT);
    status |= 0x02;   // enable IRQ12
    status &= ~0x20;  // clear "disable mouse" bit
    mouse_wait_write();
    outb(MOUSE_CMD_PORT, 0x60);
    mouse_wait_write();
    outb(MOUSE_DATA_PORT, status);

    // Reset mouse
    mouse_write(0xFF);
    mouse_read();  // ACK
    mouse_read();  // 0xAA
    mouse_read();  // 0x00

    // Set defaults
    mouse_write(0xF6);  mouse_read();
    // Enable streaming
    mouse_write(0xF4);  mouse_read();

    // Register IRQ12 handler
    idt_register_handler(0x20 + MOUSE_IRQ, mouse_irq_handler);
    pic_unmask_irq(MOUSE_IRQ);
}

bool mouse_has_event(void) {
    return g_q_head != g_q_tail;
}

mouse_event_t mouse_get_event(void) {
    mouse_event_t ev = {0};
    if (g_q_head != g_q_tail) {
        ev = g_queue[g_q_head];
        g_q_head = (g_q_head + 1) % MOUSE_QUEUE_SIZE;
    }
    return ev;
}

int32_t mouse_x(void)         { return g_mouse_x; }
int32_t mouse_y(void)         { return g_mouse_y; }
bool    mouse_btn_left(void)  { return g_btn_left; }
bool    mouse_btn_right(void) { return g_btn_right; }
