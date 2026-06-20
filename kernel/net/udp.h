// =============================================================================
// Eclipse32 - UDP (RFC 768)
// Minimal: no real socket table, just per-local-port receive callbacks.
// Enough to support a DNS client (and later anything else UDP-based).
// =============================================================================
#pragma once
#include "net.h"

#pragma pack(push, 1)
typedef struct {
    uint16_t src_port;    // big-endian
    uint16_t dst_port;    // big-endian
    uint16_t length;      // big-endian, header + data
    uint16_t checksum;    // big-endian (0 = not computed, which is legal for IPv4/UDP)
} udp_header_t;
#pragma pack(pop)

#define UDP_HEADER_LEN  8

// Called when a UDP datagram arrives addressed to `local_port`.
typedef void (*udp_recv_cb_t)(ipv4_addr_t src_ip, uint16_t src_port,
                              const uint8_t *data, uint16_t len, void *ctx);

// Register interest in datagrams arriving on `local_port`. Only one callback
// per port — registering again on the same port replaces the previous one.
// `ctx` is passed back unchanged to the callback (lets one handler serve
// multiple concurrent requests if needed later).
void udp_bind(uint16_t local_port, udp_recv_cb_t cb, void *ctx);
void udp_unbind(uint16_t local_port);

// Send a UDP datagram.
bool udp_send(ipv4_addr_t dst_ip, uint16_t src_port, uint16_t dst_port,
              const void *data, uint16_t len);

// Dispatch entry point called by ipv4_handle_packet() for IPV4_PROTO_UDP.
void udp_handle_packet(ipv4_addr_t src_ip, const uint8_t *data, uint16_t len);
