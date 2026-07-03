// =============================================================================
// Eclipse32 - IPv4 (RFC 791, minimal: no fragmentation, no options, no routing
// table — either send directly to a host on our /24, or via the gateway)
// =============================================================================
#include "ipv4.h"
#include "eth.h"
#include "arp.h"
#include "icmp.h"
#include "udp.h"

#include "tcp.h"

extern mac_addr_t  net_get_mac(void);
extern ipv4_addr_t net_get_ip(void);
extern ipv4_addr_t net_get_gateway(void);

static uint16_t next_identification = 1;

// We don't track a configurable subnet mask yet — assume a /24, which is
// QEMU user-mode networking's default (10.0.2.0/24) and the common case for
// simple test setups. Anything outside our own /24 routes via the gateway.
static bool same_subnet_24(ipv4_addr_t a, ipv4_addr_t b) {
    return a.b[0] == b.b[0] && a.b[1] == b.b[1] && a.b[2] == b.b[2];
}

bool ipv4_send(ipv4_addr_t dst, uint8_t protocol, const void *payload, uint16_t payload_len) {
    if (payload_len > (ETH_MTU - IPV4_HEADER_LEN)) return false; // no fragmentation support

    ipv4_addr_t my_ip = net_get_ip();
    ipv4_addr_t gateway = net_get_gateway();

    // Decide next hop: direct if on our subnet, otherwise via gateway.
    ipv4_addr_t next_hop = same_subnet_24(dst, my_ip) ? dst : gateway;

    mac_addr_t next_hop_mac;
    if (!arp_resolve(next_hop, &next_hop_mac)) {
        // Not resolved yet (request just sent or still pending) — caller
        // should retry; we don't block waiting for it.
        return false;
    }

    uint8_t packet[ETH_MTU];
    ipv4_header_t *hdr = (ipv4_header_t *)packet;

    hdr->version_ihl   = (4 << 4) | (IPV4_HEADER_LEN / 4);
    hdr->dscp_ecn       = 0;
    hdr->total_length   = net_htons((uint16_t)(IPV4_HEADER_LEN + payload_len));
    hdr->identification = net_htons(next_identification++);
    hdr->flags_fragment = net_htons(0x4000); // "don't fragment" set, no offset — we never fragment anyway
    hdr->ttl            = 64;
    hdr->protocol       = protocol;
    hdr->checksum       = 0; // computed below, over header only
    for (int i = 0; i < IPV4_ADDR_LEN; i++) {
        hdr->src[i] = my_ip.b[i];
        hdr->dst[i] = dst.b[i];
    }
    hdr->checksum = net_htons(net_checksum16(hdr, IPV4_HEADER_LEN));

    for (uint16_t i = 0; i < payload_len; i++) {
        packet[IPV4_HEADER_LEN + i] = ((const uint8_t *)payload)[i];
    }

    return eth_send(next_hop_mac, ETHERTYPE_IPV4, packet, (uint16_t)(IPV4_HEADER_LEN + payload_len));
}

void ipv4_handle_packet(const uint8_t *data, uint16_t len) {
    if (len < IPV4_HEADER_LEN) return;

    const ipv4_header_t *hdr = (const ipv4_header_t *)data;

    uint8_t version = hdr->version_ihl >> 4;
    uint8_t ihl_words = hdr->version_ihl & 0x0F;
    uint16_t ihl_bytes = (uint16_t)(ihl_words * 4);

    if (version != 4) return;
    if (ihl_bytes < IPV4_HEADER_LEN || ihl_bytes > len) return;

    // Verify checksum: a correct header checksums to 0 when the stored
    // checksum field is included in the sum.
    if (net_checksum16(hdr, ihl_bytes) != 0) return;

    ipv4_addr_t my_ip = net_get_ip();
    ipv4_addr_t dst_ip = ipv4_make(hdr->dst[0], hdr->dst[1], hdr->dst[2], hdr->dst[3]);
    if (!ipv4_eq(dst_ip, my_ip)) return; // not addressed to us; we're not a router

    ipv4_addr_t src_ip = ipv4_make(hdr->src[0], hdr->src[1], hdr->src[2], hdr->src[3]);

    uint16_t total_len = net_ntohs(hdr->total_length);
    if (total_len > len) return; // truncated frame, ignore

    const uint8_t *payload = data + ihl_bytes;
    uint16_t payload_len = (uint16_t)(total_len - ihl_bytes);

    if (hdr->protocol == IPV4_PROTO_ICMP) {
        icmp_handle_packet(src_ip, payload, payload_len);
    } else if (hdr->protocol == IPV4_PROTO_UDP) {
        udp_handle_packet(src_ip, payload, payload_len);
    } else if (hdr->protocol == IPV4_PROTO_TCP) {
        tcp_handle_packet(src_ip, payload, payload_len);
    }
}
