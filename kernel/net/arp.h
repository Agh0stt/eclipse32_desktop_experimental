// =============================================================================
// Eclipse32 - ARP (Address Resolution Protocol, RFC 826)
// =============================================================================
#pragma once
#include "net.h"

// Resolve an IPv4 address to a MAC address. Returns true and fills *out_mac
// if already cached; otherwise sends an ARP request and returns false
// immediately (caller should retry after net_poll() has had a chance to
// process the reply — this is intentionally non-blocking so it never stalls
// the GUI loop).
bool arp_resolve(ipv4_addr_t ip, mac_addr_t *out_mac);

// Manually seed the cache (used once for the gateway after DHCP/static config,
// optional otherwise — arp_resolve() will populate it lazily either way).
void arp_cache_insert(ipv4_addr_t ip, mac_addr_t mac);

// Dispatch entry point called by eth_handle_frame() for ETHERTYPE_ARP frames.
void arp_handle_packet(const uint8_t *data, uint16_t len, mac_addr_t src_mac);

// Call periodically (from net_poll) to retransmit pending ARP requests
// that haven't been answered yet.
void arp_tick(void);
