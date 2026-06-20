// =============================================================================
// Eclipse32 - RTL8139 Fast Ethernet NIC Driver
// =============================================================================
#pragma once
#include "../../kernel.h"
#include "../../net/net.h"

// Returns true if an RTL8139 was found, initialized, and is ready to use.
bool rtl8139_init(void);

bool rtl8139_present(void);
mac_addr_t rtl8139_get_mac(void);

// Send a raw Ethernet frame (caller fills dest MAC, src MAC, ethertype, payload).
// Returns true on success. len must be <= 1514 (standard MTU + 14-byte header).
bool rtl8139_send(const void *frame, uint16_t len);

// Poll for received frames. Calls `cb(frame_data, frame_len)` once per frame
// currently sitting in the receive ring. Safe to call with no frames pending
// (no-op). This is poll-based rather than purely interrupt-driven so net_poll()
// can be called from the GUI loop without depending on IRQ delivery timing.
typedef void (*rtl8139_rx_cb_t)(const uint8_t *data, uint16_t len);
void rtl8139_poll(rtl8139_rx_cb_t cb);
