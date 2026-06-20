// =============================================================================
// Eclipse32 - ICMP (RFC 792) — echo request/reply ("ping")
// =============================================================================
#pragma once
#include "net.h"

#pragma pack(push, 1)
typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;     // big-endian
    uint16_t identifier;   // big-endian
    uint16_t sequence;     // big-endian
} icmp_header_t;
#pragma pack(pop)

#define ICMP_TYPE_ECHO_REQUEST  8
#define ICMP_TYPE_ECHO_REPLY    0

// Send an ICMP echo request ("ping") to `dst`. `seq` is whatever sequence
// number you want in the request (shows up matched in the reply).
bool icmp_send_ping(ipv4_addr_t dst, uint16_t identifier, uint16_t seq);

// Dispatch entry point called by ipv4_handle_packet() for IPV4_PROTO_ICMP.
void icmp_handle_packet(ipv4_addr_t src, const uint8_t *data, uint16_t len);

// Set a callback to be notified when an echo reply comes back, so the shell
// can implement a `ping` command that prints round-trip results.
typedef void (*icmp_reply_cb_t)(ipv4_addr_t from, uint16_t identifier, uint16_t seq, uint32_t rtt_ms);
void icmp_set_reply_callback(icmp_reply_cb_t cb);
