// =============================================================================
// Eclipse32 - DNS client (RFC 1035, minimal: single in-flight A record query)
// =============================================================================
#include "dns.h"
#include "udp.h"
#include "net.h"
#include "../arch/x86/pit.h"
#include "../initramfs/initramfs.h"   // kstrlen, kstrchr

#define DNS_SERVER_PORT   53
#define DNS_LOCAL_PORT    49152   // arbitrary ephemeral port, out of the "well-known" range
#define DNS_QTYPE_A       1
#define DNS_QCLASS_IN     1
#define DNS_MAX_PACKET    512     // plenty for a single-question A-record query/response

extern ipv4_addr_t net_get_dns_server(void);

#pragma pack(push, 1)
typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;
#pragma pack(pop)

#define DNS_FLAG_QR        0x8000  // 1 = response
#define DNS_FLAG_RCODE_MASK 0x000F

static uint16_t next_query_id = 0x1234;

static bool         query_pending = false;
static uint16_t     pending_id = 0;
static bool         result_ready = false;
static ipv4_addr_t  result_ip;
static bool         result_found = false; // false if NXDOMAIN/error rather than a real answer

// -----------------------------------------------------------------------------
// QNAME encoding: "claude.ai" -> 06 'c''l''a''u''d''e' 02 'a''i' 00
// -----------------------------------------------------------------------------
static int encode_qname(const char *hostname, uint8_t *out, int max_len) {
    int out_pos = 0;
    const char *label_start = hostname;

    while (true) {
        const char *dot = kstrchr(label_start, '.');
        int label_len = dot ? (int)(dot - label_start) : (int)kstrlen(label_start);

        if (label_len <= 0 || label_len > 63) return -1;       // empty or too-long label
        if (out_pos + 1 + label_len > max_len) return -1;       // would overflow buffer

        out[out_pos++] = (uint8_t)label_len;
        for (int i = 0; i < label_len; i++) out[out_pos++] = (uint8_t)label_start[i];

        if (!dot) break;
        label_start = dot + 1;
        if (*label_start == '\0') break; // trailing dot, e.g. "claude.ai." — just stop
    }

    if (out_pos + 1 > max_len) return -1;
    out[out_pos++] = 0; // root label terminator
    return out_pos;
}

// Skip over a (possibly compressed) DNS name starting at `data[pos]`, within
// a packet of total length `len`. Returns the offset just past the name, or
// -1 on malformed input. Does not decode the name — callers that need the
// actual resolved name would walk separately; we only need to skip past it
// in the answer records here since we already know what we asked for.
static int skip_name(const uint8_t *data, uint16_t len, int pos) {
    while (true) {
        if (pos < 0 || pos >= len) return -1;
        uint8_t b = data[pos];

        if (b == 0) {
            return pos + 1; // root label, name ends here
        } else if ((b & 0xC0) == 0xC0) {
            // Compression pointer: 2 bytes total, points elsewhere in the
            // packet for the rest of the name — but for *skipping purposes*
            // the pointer itself is only 2 bytes here, regardless of what
            // it points to.
            if (pos + 1 >= len) return -1;
            return pos + 2;
        } else if ((b & 0xC0) == 0x00) {
            int label_len = b;
            pos += 1 + label_len;
        } else {
            return -1; // reserved/invalid label type
        }
    }
}

static void dns_recv_handler(ipv4_addr_t src_ip, uint16_t src_port,
                              const uint8_t *data, uint16_t len, void *ctx) {
    (void)src_port; (void)ctx; (void)src_ip;
    if (!query_pending) return;
    if (len < sizeof(dns_header_t)) return;

    const dns_header_t *hdr = (const dns_header_t *)data;
    uint16_t id = net_ntohs(hdr->id);
    if (id != pending_id) return; // not the reply we're waiting for

    uint16_t flags = net_ntohs(hdr->flags);
    if (!(flags & DNS_FLAG_QR)) return; // not actually a response

    uint16_t qdcount = net_ntohs(hdr->qdcount);
    uint16_t ancount = net_ntohs(hdr->ancount);
    uint8_t rcode = (uint8_t)(flags & DNS_FLAG_RCODE_MASK);

    query_pending = false;
    result_ready = true;

    if (rcode != 0 || ancount == 0) {
        result_found = false; // NXDOMAIN or empty answer
        return;
    }

    // Walk past the question section (we sent exactly one question).
    int pos = sizeof(dns_header_t);
    for (uint16_t i = 0; i < qdcount; i++) {
        pos = skip_name(data, len, pos);
        if (pos < 0 || pos + 4 > len) { result_found = false; return; }
        pos += 4; // QTYPE(2) + QCLASS(2)
    }

    // Walk the answer section looking for the first A record.
    for (uint16_t i = 0; i < ancount; i++) {
        pos = skip_name(data, len, pos);
        if (pos < 0 || pos + 10 > len) { result_found = false; return; }

        uint16_t rtype  = (uint16_t)((data[pos] << 8) | data[pos + 1]);
        uint16_t rclass = (uint16_t)((data[pos + 2] << 8) | data[pos + 3]);
        uint16_t rdlen  = (uint16_t)((data[pos + 8] << 8) | data[pos + 9]);
        pos += 10;

        if (pos + rdlen > len) { result_found = false; return; }

        if (rtype == DNS_QTYPE_A && rclass == DNS_QCLASS_IN && rdlen == 4) {
            result_ip = ipv4_make(data[pos], data[pos+1], data[pos+2], data[pos+3]);
            result_found = true;
            return;
        }

        pos += rdlen; // skip CNAME/other record types we don't care about
    }

    result_found = false; // no A record among the answers (e.g. CNAME-only chain)
}

bool dns_query(const char *hostname) {
    uint8_t packet[DNS_MAX_PACKET];
    dns_header_t *hdr = (dns_header_t *)packet;

    uint16_t qid = next_query_id++;

    hdr->id      = net_htons(qid);
    hdr->flags   = net_htons(0x0100); // standard query, recursion desired
    hdr->qdcount = net_htons(1);
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;

    int name_len = encode_qname(hostname, packet + sizeof(dns_header_t),
                                 (int)(DNS_MAX_PACKET - sizeof(dns_header_t) - 4));
    if (name_len < 0) return false;

    int pos = (int)sizeof(dns_header_t) + name_len;
    packet[pos++] = 0x00; packet[pos++] = DNS_QTYPE_A;   // QTYPE = A
    packet[pos++] = 0x00; packet[pos++] = DNS_QCLASS_IN; // QCLASS = IN

    udp_bind(DNS_LOCAL_PORT, dns_recv_handler, NULL);

    ipv4_addr_t dns_server = net_get_dns_server();
    bool ok = udp_send(dns_server, DNS_LOCAL_PORT, DNS_SERVER_PORT, packet, (uint16_t)pos);
    if (ok) {
        query_pending = true;
        pending_id = qid;
        result_ready = false;
    }
    return ok;
}

bool dns_poll_result(ipv4_addr_t *out_ip) {
    if (!result_ready) return false;
    result_ready = false; // consume — only reported once

    if (result_found && out_ip) *out_ip = result_ip;
    return result_found;
}

bool dns_resolve_blocking(const char *hostname, ipv4_addr_t *out_ip, uint32_t timeout_ms) {
    uint32_t deadline = pit_ms() + timeout_ms;
    bool sent = dns_query(hostname);
    while (!sent && pit_ms() < deadline) { net_poll(); sent = dns_query(hostname); }
    if (!sent) return false;
    while (pit_ms() < deadline) {
        net_poll();
        if (result_ready) {
            return dns_poll_result(out_ip);
        }
    }
    query_pending = false; // give up
    return false;
}
