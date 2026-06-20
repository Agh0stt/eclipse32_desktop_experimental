// =============================================================================
// Eclipse32 - Ethernet II framing
// =============================================================================
#pragma once
#include "net.h"

#define ETH_HEADER_LEN   14
#define ETH_MTU          1500
#define ETH_FRAME_MAX    (ETH_HEADER_LEN + ETH_MTU)

#define ETHERTYPE_IPV4   0x0800
#define ETHERTYPE_ARP    0x0806

#pragma pack(push, 1)
typedef struct {
    uint8_t  dst[MAC_ADDR_LEN];
    uint8_t  src[MAC_ADDR_LEN];
    uint16_t ethertype;   // big-endian on the wire; use net_ntohs() to read
} eth_header_t;
#pragma pack(pop)

// Build and send an Ethernet II frame. `payload`/`payload_len` is whatever
// sits after the 14-byte header (an ARP packet or IPv4 packet, etc).
bool eth_send(mac_addr_t dst, uint16_t ethertype, const void *payload, uint16_t payload_len);

// Called by the NIC driver's poll callback for every received frame.
// Dispatches to ARP/IPv4 based on ethertype.
void eth_handle_frame(const uint8_t *data, uint16_t len);
