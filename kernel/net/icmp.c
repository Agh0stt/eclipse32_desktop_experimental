// =============================================================================
// Eclipse32 - ICMP (RFC 792) — echo request/reply ("ping")
// =============================================================================
#include "icmp.h"
#include "ipv4.h"
#include "../arch/x86/pit.h"

#define ICMP_PAYLOAD_LEN  32   // arbitrary small payload, like classic `ping`
#define ICMP_MAX_PENDING  8

typedef struct {
    bool        used;
    uint16_t    identifier;
    uint16_t    sequence;
    uint32_t    sent_at_ms;
} pending_ping_t;

static pending_ping_t pending[ICMP_MAX_PENDING];
static icmp_reply_cb_t reply_callback = NULL;

void icmp_set_reply_callback(icmp_reply_cb_t cb) {
    reply_callback = cb;
}

static void remember_pending(uint16_t identifier, uint16_t seq) {
    for (int i = 0; i < ICMP_MAX_PENDING; i++) {
        if (!pending[i].used) {
            pending[i].used = true;
            pending[i].identifier = identifier;
            pending[i].sequence = seq;
            pending[i].sent_at_ms = pit_ms();
            return;
        }
    }
    // Table full: overwrite the oldest-looking slot (index 0). Fine for a
    // simple ping command that sends one at a time.
    pending[0].used = true;
    pending[0].identifier = identifier;
    pending[0].sequence = seq;
    pending[0].sent_at_ms = pit_ms();
}

bool icmp_send_ping(ipv4_addr_t dst, uint16_t identifier, uint16_t seq) {
    uint8_t packet[sizeof(icmp_header_t) + ICMP_PAYLOAD_LEN];
    icmp_header_t *hdr = (icmp_header_t *)packet;

    hdr->type       = ICMP_TYPE_ECHO_REQUEST;
    hdr->code       = 0;
    hdr->checksum   = 0;
    hdr->identifier = net_htons(identifier);
    hdr->sequence   = net_htons(seq);

    // Fill payload with a simple incrementing pattern, same spirit as the
    // classic Unix `ping` utility — content doesn't matter, just needs to
    // round-trip unchanged so we could (later) verify it if we wanted to.
    for (int i = 0; i < ICMP_PAYLOAD_LEN; i++) {
        packet[sizeof(icmp_header_t) + i] = (uint8_t)i;
    }

    hdr->checksum = net_htons(net_checksum16(packet, sizeof(packet)));

    bool ok = ipv4_send(dst, IPV4_PROTO_ICMP, packet, sizeof(packet));
    if (ok) remember_pending(identifier, seq);
    return ok;
}

static void send_echo_reply(ipv4_addr_t dst, const icmp_header_t *req_hdr,
                             const uint8_t *req_payload, uint16_t req_payload_len) {
    uint8_t packet[sizeof(icmp_header_t) + 1500];
    if (req_payload_len > 1500) req_payload_len = 1500; // clamp, shouldn't happen with std MTU

    icmp_header_t *hdr = (icmp_header_t *)packet;
    hdr->type       = ICMP_TYPE_ECHO_REPLY;
    hdr->code       = 0;
    hdr->checksum   = 0;
    hdr->identifier = req_hdr->identifier; // already big-endian, just echo back
    hdr->sequence   = req_hdr->sequence;

    for (uint16_t i = 0; i < req_payload_len; i++) {
        packet[sizeof(icmp_header_t) + i] = req_payload[i];
    }

    uint16_t total = (uint16_t)(sizeof(icmp_header_t) + req_payload_len);
    hdr->checksum = net_htons(net_checksum16(packet, total));

    ipv4_send(dst, IPV4_PROTO_ICMP, packet, total);
}

void icmp_handle_packet(ipv4_addr_t src, const uint8_t *data, uint16_t len) {
    if (len < sizeof(icmp_header_t)) return;

    const icmp_header_t *hdr = (const icmp_header_t *)data;
    const uint8_t *payload = data + sizeof(icmp_header_t);
    uint16_t payload_len = (uint16_t)(len - sizeof(icmp_header_t));

    if (hdr->type == ICMP_TYPE_ECHO_REQUEST) {
        // Someone's pinging us — answer it. This is what makes Eclipse32
        // itself visible to `ping` from the host/other machines, separate
        // from us originating pings outward.
        send_echo_reply(src, hdr, payload, payload_len);
        return;
    }

    if (hdr->type == ICMP_TYPE_ECHO_REPLY) {
        uint16_t identifier = net_ntohs(hdr->identifier);
        uint16_t seq = net_ntohs(hdr->sequence);

        for (int i = 0; i < ICMP_MAX_PENDING; i++) {
            if (pending[i].used && pending[i].identifier == identifier && pending[i].sequence == seq) {
                uint32_t rtt = pit_ms() - pending[i].sent_at_ms;
                pending[i].used = false;
                if (reply_callback) reply_callback(src, identifier, seq, rtt);
                return;
            }
        }
        // Reply to something we don't have a record of (e.g. after a cache
        // overwrite) — still worth surfacing without an RTT.
        if (reply_callback) reply_callback(src, identifier, seq, 0);
    }
}
