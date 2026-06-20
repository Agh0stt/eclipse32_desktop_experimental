// =============================================================================
// Eclipse32 - PS/2 Mouse Driver Header
// =============================================================================
#pragma once
#include "../../kernel.h"

typedef struct {
    int32_t  dx;        // relative X movement (signed)
    int32_t  dy;        // relative Y movement (signed, Y-flipped)
    bool     btn_left;
    bool     btn_right;
    bool     btn_middle;
} mouse_event_t;

void  mouse_init(void);
bool  mouse_has_event(void);
mouse_event_t mouse_get_event(void);

// Absolute cursor position (clamped to screen)
int32_t mouse_x(void);
int32_t mouse_y(void);
bool    mouse_btn_left(void);
bool    mouse_btn_right(void);
