// =============================================================================
// Eclipse32 - PIT Header
// =============================================================================
#pragma once
#include "../../kernel.h"

void     pit_init(uint32_t hz);
uint32_t pit_ticks(void);
uint32_t pit_ms(void);
void     pit_sleep_ms(uint32_t ms);
