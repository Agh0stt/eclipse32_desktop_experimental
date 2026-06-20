// =============================================================================
// Eclipse32 - IPv4 (RFC 791, minimal subset: no fragmentation, no options)
// =============================================================================
#pragma once
#include "net.h"

#define IPV4_PROTO_ICMP  1
#define IPV4_PROTO_TCP   6
#define IPV4_PROTO_UDP   17

#pragma pack(push, 1)
typedef struct {
    uint8_t  version_ihl;     // upper nibble = version (4), lower = header len in 32-bit words
    uint8_t  dscp_ecn;
    uint16_t total_length;    // big-endian
    uint16_t identification;  // big-endian
    uint16_t flags_fragment;  // big-endian
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;        // big-endian
    uint8_t  src[IPV4_ADDR_LEN];
    uint8_t  dst[IPV4_ADDR_LEN];
} ipv4_header_t;
#pragma pack(pop)

#define IPV4_HEADER_LEN  20   // no options support, so always fixed at 20 bytes

// Send an IPv4 packet. Resolves the next-hop MAC via ARP automatically
// (route-to-gateway if `dst` isn't on the local /24, since we don't yet
// implement a real routing table or subnet mask config). Returns false if
// ARP resolution hasn't completed yet — caller may retry on a later poll.
bool ipv4_send(ipv4_addr_t dst, uint8_t protocol, const void *payload, uint16_t payload_len);

// Dispatch entry point called by eth_handle_frame() for ETHERTYPE_IPV4 frames.
void ipv4_handle_packet(const uint8_t *data, uint16_t len);
