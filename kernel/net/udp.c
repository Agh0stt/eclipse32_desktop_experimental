// =============================================================================
// Eclipse32 - UDP (RFC 768)
// =============================================================================
#include "udp.h"
#include "ipv4.h"

extern ipv4_addr_t net_get_ip(void);

#define UDP_MAX_BINDINGS  8

typedef struct {
    bool          used;
    uint16_t      port;
    udp_recv_cb_t cb;
    void         *ctx;
} udp_binding_t;

static udp_binding_t bindings[UDP_MAX_BINDINGS];

void udp_bind(uint16_t local_port, udp_recv_cb_t cb, void *ctx) {
    // Replace existing binding on the same port, if any.
    for (int i = 0; i < UDP_MAX_BINDINGS; i++) {
        if (bindings[i].used && bindings[i].port == local_port) {
            bindings[i].cb = cb;
            bindings[i].ctx = ctx;
            return;
        }
    }
    for (int i = 0; i < UDP_MAX_BINDINGS; i++) {
        if (!bindings[i].used) {
            bindings[i].used = true;
            bindings[i].port = local_port;
            bindings[i].cb = cb;
            bindings[i].ctx = ctx;
            return;
        }
    }
    // No free slot — silently dropped; UDP_MAX_BINDINGS=8 is generous for
    // what this stack does today (one DNS query in flight at a time, etc).
}

void udp_unbind(uint16_t local_port) {
    for (int i = 0; i < UDP_MAX_BINDINGS; i++) {
        if (bindings[i].used && bindings[i].port == local_port) {
            bindings[i].used = false;
            bindings[i].cb = NULL;
            bindings[i].ctx = NULL;
            return;
        }
    }
}

// UDP checksum is computed over a "pseudo-header" (src/dst IP, protocol,
// UDP length) followed by the real UDP header+data — per RFC 768. This is
// the same trick TCP uses; the pseudo-header isn't actually transmitted,
// it's just folded into the checksum math so corruption of the IP addresses
// in transit is also caught.
static uint16_t udp_checksum(ipv4_addr_t src, ipv4_addr_t dst, const udp_header_t *hdr,
                              const uint8_t *payload, uint16_t payload_len) {
    uint8_t pseudo[12 + UDP_HEADER_LEN + 1500]; // 1500 covers any realistic UDP payload here (DNS is tiny)
    if (payload_len > 1500) payload_len = 1500;

    uint32_t off = 0;
    for (int i = 0; i < 4; i++) pseudo[off++] = src.b[i];
    for (int i = 0; i < 4; i++) pseudo[off++] = dst.b[i];
    pseudo[off++] = 0;                       // zero
    pseudo[off++] = IPV4_PROTO_UDP;          // protocol
    uint16_t udp_len = (uint16_t)(UDP_HEADER_LEN + payload_len);
    pseudo[off++] = (uint8_t)(udp_len >> 8);
    pseudo[off++] = (uint8_t)(udp_len & 0xFF);

    // UDP header (checksum field must read as 0 while computing). hdr's
    // fields are already stored in network byte order (set via net_htons()
    // before this is called) — copy the raw bytes directly. Re-deriving
    // "high byte / low byte" via >>8 and &0xFF here would be operating on
    // an already-byte-swapped 16-bit value and silently re-swap it back,
    // corrupting the checksum (this was an actual bug: it made every UDP
    // checksum wrong, so receivers silently dropped the packet).
    const uint8_t *hdr_bytes = (const uint8_t *)hdr;
    pseudo[off++] = hdr_bytes[0]; // src_port high byte (as transmitted)
    pseudo[off++] = hdr_bytes[1]; // src_port low byte
    pseudo[off++] = hdr_bytes[2]; // dst_port high byte
    pseudo[off++] = hdr_bytes[3]; // dst_port low byte
    pseudo[off++] = hdr_bytes[4]; // length high byte
    pseudo[off++] = hdr_bytes[5]; // length low byte
    pseudo[off++] = 0; // checksum hi (zero for computation)
    pseudo[off++] = 0; // checksum lo

    for (uint16_t i = 0; i < payload_len; i++) pseudo[off++] = payload[i];

    uint16_t sum = net_checksum16(pseudo, off);
    // RFC 768: a computed checksum of exactly 0 is sent as all-ones (0xFFFF)
    // since 0 in the field means "no checksum was computed".
    return (sum == 0) ? 0xFFFF : sum;
}

bool udp_send(ipv4_addr_t dst_ip, uint16_t src_port, uint16_t dst_port,
              const void *data, uint16_t len) {
    if (len > 1500) return false;

    uint8_t packet[UDP_HEADER_LEN + 1500];
    udp_header_t *hdr = (udp_header_t *)packet;

    hdr->src_port = net_htons(src_port);
    hdr->dst_port = net_htons(dst_port);
    hdr->length   = net_htons((uint16_t)(UDP_HEADER_LEN + len));
    hdr->checksum = 0;

    for (uint16_t i = 0; i < len; i++) packet[UDP_HEADER_LEN + i] = ((const uint8_t *)data)[i];

    ipv4_addr_t my_ip = net_get_ip();
    hdr->checksum = net_htons(udp_checksum(my_ip, dst_ip, hdr, (const uint8_t *)data, len));

    return ipv4_send(dst_ip, IPV4_PROTO_UDP, packet, (uint16_t)(UDP_HEADER_LEN + len));
}

void udp_handle_packet(ipv4_addr_t src_ip, const uint8_t *data, uint16_t len) {
    if (len < UDP_HEADER_LEN) return;

    const udp_header_t *hdr = (const udp_header_t *)data;
    uint16_t src_port = net_ntohs(hdr->src_port);
    uint16_t dst_port = net_ntohs(hdr->dst_port);
    uint16_t udp_len = net_ntohs(hdr->length);

    if (udp_len < UDP_HEADER_LEN || udp_len > len) return; // malformed/truncated

    // Note: we don't verify the checksum on receive. Plenty of real stacks
    // skip this for performance; we skip it because we'd need to reconstruct
    // the same pseudo-header math here, and a corrupt frame would already
    // be vanishingly unlikely to also fail the link-layer (Ethernet) CRC
    // check without the NIC discarding it first.
    const uint8_t *payload = data + UDP_HEADER_LEN;
    uint16_t payload_len = (uint16_t)(udp_len - UDP_HEADER_LEN);

    for (int i = 0; i < UDP_MAX_BINDINGS; i++) {
        if (bindings[i].used && bindings[i].port == dst_port) {
            bindings[i].cb(src_ip, src_port, payload, payload_len, bindings[i].ctx);
            return;
        }
    }
    // No one's listening on this port — silently dropped (standard UDP behavior
    // would ICMP "port unreachable" back, which we don't implement yet).
}
