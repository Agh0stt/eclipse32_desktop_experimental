// =============================================================================
// Eclipse32 - Network stack shared types
// =============================================================================
#pragma once
#include "../kernel.h"

#define MAC_ADDR_LEN  6
#define IPV4_ADDR_LEN 4

typedef struct { uint8_t b[MAC_ADDR_LEN]; }  mac_addr_t;
typedef struct { uint8_t b[IPV4_ADDR_LEN]; } ipv4_addr_t;

static INLINE mac_addr_t mac_broadcast(void) {
    mac_addr_t m;
    for (int i = 0; i < MAC_ADDR_LEN; i++) m.b[i] = 0xFF;
    return m;
}

static INLINE bool mac_eq(mac_addr_t a, mac_addr_t b) {
    for (int i = 0; i < MAC_ADDR_LEN; i++) if (a.b[i] != b.b[i]) return false;
    return true;
}

static INLINE bool ipv4_eq(ipv4_addr_t a, ipv4_addr_t b) {
    return a.b[0]==b.b[0] && a.b[1]==b.b[1] && a.b[2]==b.b[2] && a.b[3]==b.b[3];
}

static INLINE ipv4_addr_t ipv4_make(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    ipv4_addr_t ip; ip.b[0]=a; ip.b[1]=b; ip.b[2]=c; ip.b[3]=d;
    return ip;
}

// Network byte order (big-endian) helpers — the CPU here is little-endian x86.
static INLINE uint16_t net_htons(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }
static INLINE uint16_t net_ntohs(uint16_t v) { return net_htons(v); }
static INLINE uint32_t net_htonl(uint32_t v) {
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) << 8)  |
           ((v & 0x00FF0000u) >> 8)  |
           ((v & 0xFF000000u) >> 24);
}
static INLINE uint32_t net_ntohl(uint32_t v) { return net_htonl(v); }

// Standard internet checksum (RFC 1071) used by IPv4 and ICMP headers.
static INLINE uint16_t net_checksum16(const void *data, uint32_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = 0;
    while (len > 1) {
        uint16_t word = (uint16_t)((p[0] << 8) | p[1]);
        sum += word;
        p += 2;
        len -= 2;
    }
    if (len == 1) {
        sum += (uint16_t)(p[0] << 8);
    }
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum & 0xFFFF);
}

// Called once at boot after the NIC driver is up. Sets the local MAC/IP and
// wires the receive callback into the ARP/IPv4 dispatch.
// Called once at boot after the NIC driver is up. Sets the local MAC/IP and
// wires the receive callback into the ARP/IPv4 dispatch. `dns_server_ip` is
// configured separately from the gateway since some setups (notably QEMU's
// "-netdev user" slirp networking) put DNS on a different address than the
// gateway/router (10.0.2.3 vs 10.0.2.2) — querying the gateway for DNS there
// gets back ICMP port-unreachable, not a DNS reply.
void net_init(mac_addr_t my_mac, ipv4_addr_t my_ip, ipv4_addr_t gateway_ip, ipv4_addr_t dns_server_ip);

// Pump the network stack — call periodically (e.g. once per gui_pump frame)
// so received frames sitting in the NIC's ring get processed even without
// interrupts, and so timed-out ARP requests can be retried.
void net_poll(void);

mac_addr_t  net_get_mac(void);
ipv4_addr_t net_get_ip(void);
ipv4_addr_t net_get_gateway(void);
ipv4_addr_t net_get_dns_server(void);
