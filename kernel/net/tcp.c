// =============================================================================
// Eclipse32 - TCP (RFC 793, minimal client-side)
// One connection at a time. Client only. No out-of-order reassembly.
// =============================================================================
#include "tcp.h"
#include "ipv4.h"
#include "net.h"
#include "../arch/x86/pit.h"
#include "../sched/sched.h"

extern ipv4_addr_t net_get_ip(void);

// ---------------------------------------------------------------------------
// TCP header
// ---------------------------------------------------------------------------
#pragma pack(push, 1)
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  data_offset;   // upper 4 bits = header len in 32-bit words
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} tcp_header_t;
#pragma pack(pop)

#define TCP_HEADER_LEN   20   // no options

// Flag bits
#define TCP_FIN  0x01
#define TCP_SYN  0x02
#define TCP_RST  0x04
#define TCP_PSH  0x08
#define TCP_ACK  0x10
#define TCP_URG  0x20

// ---------------------------------------------------------------------------
// Connection state
// ---------------------------------------------------------------------------
static tcp_state_t  g_state      = TCP_CLOSED;
static ipv4_addr_t  g_remote_ip;
static uint16_t     g_remote_port;
static uint16_t     g_local_port;
static uint32_t     g_seq;        // next seq we'll send
static uint32_t     g_ack;        // next seq we expect from remote
static uint32_t     g_remote_isn; // remote initial seq (for tracking)
static uint16_t     g_remote_win; // remote's advertised window
static bool         g_got_fin;    // remote sent FIN

// Receive ring buffer
static uint8_t  g_recv_buf[TCP_RECV_BUF_SIZE];
static uint32_t g_recv_head;   // write pos
static uint32_t g_recv_tail;   // read pos
static uint32_t g_recv_count;

// Last-sent unacknowledged segment (for retransmit)
static uint8_t  g_retx_buf[TCP_SEND_BUF_SIZE];
static uint16_t g_retx_len;
static uint32_t g_retx_seq;
static uint32_t g_retx_deadline_ms;
static int      g_retx_tries;

// Ephemeral port counter
static uint16_t g_ephemeral = 49200;

// ---------------------------------------------------------------------------
// Checksum (TCP pseudo-header)
// ---------------------------------------------------------------------------
static uint16_t tcp_checksum(ipv4_addr_t src, ipv4_addr_t dst,
                              const tcp_header_t *hdr,
                              const uint8_t *payload, uint16_t payload_len) {
    uint16_t tcp_len = (uint16_t)(TCP_HEADER_LEN + payload_len);
    uint8_t pseudo[12 + TCP_HEADER_LEN + TCP_SEND_BUF_SIZE];
    uint32_t off = 0;

    for (int i = 0; i < 4; i++) pseudo[off++] = src.b[i];
    for (int i = 0; i < 4; i++) pseudo[off++] = dst.b[i];
    pseudo[off++] = 0;
    pseudo[off++] = IPV4_PROTO_TCP;
    pseudo[off++] = (uint8_t)(tcp_len >> 8);
    pseudo[off++] = (uint8_t)(tcp_len & 0xFF);

    // Copy header with checksum zeroed
    const uint8_t *hb = (const uint8_t *)hdr;
    for (int i = 0; i < TCP_HEADER_LEN; i++) pseudo[off++] = hb[i];
    // Zero checksum field (bytes 16-17 of header)
    pseudo[off - 4] = 0;
    pseudo[off - 3] = 0;

    for (uint16_t i = 0; i < payload_len; i++) pseudo[off++] = payload[i];

    uint16_t sum = net_checksum16(pseudo, off);
    return (sum == 0) ? 0xFFFF : sum;
}

// ---------------------------------------------------------------------------
// Send a TCP segment
// ---------------------------------------------------------------------------
static bool tcp_send_segment(uint8_t flags, uint32_t seq,
                              const void *payload, uint16_t payload_len) {
    if (payload_len > TCP_SEND_BUF_SIZE) return false;

    uint8_t packet[TCP_HEADER_LEN + TCP_SEND_BUF_SIZE];
    tcp_header_t *hdr = (tcp_header_t *)packet;

    hdr->src_port    = net_htons(g_local_port);
    hdr->dst_port    = net_htons(g_remote_port);
    hdr->seq         = net_htonl(seq);
    hdr->ack         = net_htonl(g_ack);
    hdr->data_offset = (TCP_HEADER_LEN / 4) << 4;
    hdr->flags       = flags;
    hdr->window      = net_htons(TCP_RECV_BUF_SIZE - g_recv_count);
    hdr->checksum    = 0;
    hdr->urgent      = 0;

    if (payload && payload_len > 0) {
        for (uint16_t i = 0; i < payload_len; i++)
            packet[TCP_HEADER_LEN + i] = ((const uint8_t *)payload)[i];
    }

    ipv4_addr_t my_ip = net_get_ip();
    hdr->checksum = net_htons(tcp_checksum(my_ip, g_remote_ip, hdr,
                                           (const uint8_t *)payload, payload_len));

    return ipv4_send(g_remote_ip, IPV4_PROTO_TCP, packet,
                     (uint16_t)(TCP_HEADER_LEN + payload_len));
}

// ---------------------------------------------------------------------------
// Push data into recv buffer
// ---------------------------------------------------------------------------
static void recv_push(const uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        if (g_recv_count >= TCP_RECV_BUF_SIZE) break; // drop if full
        g_recv_buf[g_recv_head] = data[i];
        g_recv_head = (g_recv_head + 1) % TCP_RECV_BUF_SIZE;
        g_recv_count++;
    }
}

// ---------------------------------------------------------------------------
// Out-of-order segment buffer (handles NAT/SLIRP reordering)
// Stores up to OOO_SLOTS segments that arrived ahead of g_ack.
// ---------------------------------------------------------------------------
#define OOO_SLOTS      8
#define OOO_SEG_MAX  1460

typedef struct {
    uint32_t seq;
    uint16_t len;
    bool     valid;
    uint8_t  data[OOO_SEG_MAX];
} ooo_seg_t;

static ooo_seg_t g_ooo[OOO_SLOTS];

static void ooo_reset(void) {
    for (int i = 0; i < OOO_SLOTS; i++) g_ooo[i].valid = false;
}

static void ooo_store(uint32_t seq, const uint8_t *data, uint16_t len) {
    if (len > OOO_SEG_MAX) return;
    for (int i = 0; i < OOO_SLOTS; i++) {
        if (!g_ooo[i].valid || g_ooo[i].seq == seq) {
            g_ooo[i].seq   = seq;
            g_ooo[i].len   = len;
            g_ooo[i].valid = true;
            for (uint16_t j = 0; j < len; j++) g_ooo[i].data[j] = data[j];
            return;
        }
    }
    // All slots full — evict oldest (slot 0) and store at end
    for (int i = 0; i < OOO_SLOTS - 1; i++) g_ooo[i] = g_ooo[i + 1];
    g_ooo[OOO_SLOTS-1].seq   = seq;
    g_ooo[OOO_SLOTS-1].len   = len;
    g_ooo[OOO_SLOTS-1].valid = true;
    for (uint16_t j = 0; j < len; j++) g_ooo[OOO_SLOTS-1].data[j] = data[j];
}

// Try to drain consecutive buffered segments starting at g_ack.
// IMPORTANT: a segment is only consumed (and g_ack advanced) if the recv
// ring has room for the *entire* segment. Partially draining a segment
// (old behavior: clamp to available space, advance g_ack by the clamped
// amount, then discard the slot anyway) silently dropped the undelivered
// tail bytes and desynced g_ack from what was actually buffered -- any
// later segment from the real next sequence number would then look
// "ahead" of g_ack and get stashed instead of delivered, with nothing
// left in g_ooo at the (now permanently wrong) g_ack value to drain it.
// That manifested as the connection hanging silently under SLIRP/NAT
// reordering once the recv ring got close to full during a download.
static void ooo_drain(void) {
    bool progress = true;
    while (progress) {
        progress = false;
        for (int i = 0; i < OOO_SLOTS; i++) {
            if (!g_ooo[i].valid) continue;
            if (g_ooo[i].seq != g_ack) continue;

            uint16_t space = (uint16_t)(TCP_RECV_BUF_SIZE - g_recv_count);
            if (g_ooo[i].len > space) {
                // Not enough room yet -- leave it buffered and stop. The
                // app will free up ring space via tcp_recv(), and we'll
                // retry on the next ooo_drain() call (e.g. next packet
                // or next tcp_tick()).
                continue;
            }

            recv_push(g_ooo[i].data, g_ooo[i].len);
            g_ack += g_ooo[i].len;
            g_ooo[i].valid = false;
            progress = true;
        }
    }
}

// ---------------------------------------------------------------------------
// tcp_handle_packet — called by ipv4_handle_packet
// ---------------------------------------------------------------------------
void tcp_handle_packet(ipv4_addr_t src_ip, const uint8_t *data, uint16_t len) {
    if (len < TCP_HEADER_LEN) return;
    if (g_state == TCP_CLOSED) return;

    const tcp_header_t *hdr = (const tcp_header_t *)data;
    uint16_t src_port = net_ntohs(hdr->src_port);
    uint16_t dst_port = net_ntohs(hdr->dst_port);

    // Must match our connection
    if (dst_port != g_local_port) return;
    if (src_port != g_remote_port) return;
    if (!ipv4_eq(src_ip, g_remote_ip)) return;

    uint8_t  flags     = hdr->flags;
    uint32_t seg_seq   = net_ntohl(hdr->seq);
    uint32_t seg_ack   = net_ntohl(hdr->ack);
    uint8_t  hdr_words = (hdr->data_offset >> 4);
    uint16_t hdr_bytes = (uint16_t)(hdr_words * 4);
    if (hdr_bytes < TCP_HEADER_LEN || hdr_bytes > len) return;

    const uint8_t *payload     = data + hdr_bytes;
    uint16_t       payload_len = (uint16_t)(len - hdr_bytes);
    g_remote_win = net_ntohs(hdr->window);

    // RST — kill connection immediately
    if (flags & TCP_RST) {
        g_state = TCP_CLOSED;
        return;
    }

    switch (g_state) {
    case TCP_SYN_SENT:
        // Expect SYN+ACK
        if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
            if (seg_ack != g_seq) return; // wrong ack
            g_remote_isn = seg_seq;
            g_ack = seg_seq + 1;
            g_state = TCP_ESTABLISHED;
            // Send ACK
            tcp_send_segment(TCP_ACK, g_seq, NULL, 0);
            // Clear retransmit
            g_retx_len = 0;
        }
        break;

    case TCP_ESTABLISHED:
    case TCP_FIN_WAIT1:
    case TCP_FIN_WAIT2:
    case TCP_CLOSE_WAIT:
        // Accept data: in-order gets pushed directly; out-of-order gets buffered.
        if (payload_len > 0) {
            if (seg_seq == g_ack) {
                // In-order: push directly into recv ring, but only if the
                // ring has room for the whole segment. Same reasoning as
                // ooo_drain() above -- clamping and advancing g_ack by less
                // than payload_len would silently drop the tail and desync
                // g_ack from the data we actually have. If there isn't
                // room, drop the segment untouched; TCP_RECV_BUF_SIZE -
                // g_recv_count is advertised as our window (see
                // tcp_send_segment), so a well-behaved peer/SLIRP won't
                // send more than that -- and if it does anyway, our
                // retransmit timer / lack of ACK will make it resend once
                // tcp_recv() drains the ring and frees up space.
                uint16_t space = (uint16_t)(TCP_RECV_BUF_SIZE - g_recv_count);
                if (payload_len <= space) {
                    recv_push(payload, payload_len);
                    g_ack += payload_len;
                    // Now try to drain any buffered segments that are now in order
                    ooo_drain();
                }
            } else if (seg_seq > g_ack) {
                // Future segment — stash it for later (handles NAT reordering)
                ooo_store(seg_seq, payload, payload_len);
            }
            // seg_seq < g_ack: duplicate/already-received, ignore
        }

        // Track remote's ACK of our data
        if ((flags & TCP_ACK) && seg_ack == g_seq) {
            g_retx_len = 0; // our segment was acked
        }

        // Remote closed its side
        if ((flags & TCP_FIN) && !g_got_fin) {
            // Drain any remaining buffered data before accepting FIN
            ooo_drain();
            g_got_fin = true;
            g_ack++;  // FIN consumes one seq number
            if (g_state == TCP_ESTABLISHED) {
                g_state = TCP_CLOSE_WAIT;
            } else if (g_state == TCP_FIN_WAIT1 || g_state == TCP_FIN_WAIT2) {
                g_state = TCP_TIME_WAIT;
            }
            tcp_send_segment(TCP_ACK, g_seq, NULL, 0);
        }

        // ACK our FIN if we sent one
        if (g_state == TCP_FIN_WAIT1 && (flags & TCP_ACK) && seg_ack == g_seq) {
            g_state = TCP_FIN_WAIT2;
        }

        // Send ACK if we received data (always, so peer knows our current g_ack)
        if (payload_len > 0) {
            tcp_send_segment(TCP_ACK, g_seq, NULL, 0);
        }
        break;

    case TCP_LAST_ACK:
        if ((flags & TCP_ACK) && seg_ack == g_seq) {
            g_state = TCP_CLOSED;
        }
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// tcp_tick — retransmit handling, called from net_poll
// ---------------------------------------------------------------------------
void tcp_tick(void) {
    if (g_state == TCP_CLOSED) return;
    if (g_retx_len == 0) return;
    if (pit_ms() < g_retx_deadline_ms) return;

    if (g_retx_tries >= TCP_MAX_RETRIES) {
        g_state = TCP_CLOSED;
        g_retx_len = 0;
        return;
    }

    // Retransmit
    g_retx_tries++;
    g_retx_deadline_ms = pit_ms() + 1000;
    tcp_send_segment(TCP_SYN | (g_state == TCP_SYN_SENT ? 0 : TCP_ACK),
                     g_retx_seq, g_retx_buf, g_retx_len);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

static void poll_until(bool (*cond)(void), uint32_t timeout_ms) {
    uint32_t deadline = pit_ms() + timeout_ms;
    while (!cond() && pit_ms() < deadline) {
        extern void net_poll(void);
        net_poll();
        if (sched_app_running()) sched_yield();
    }
}

static bool is_established(void) { return g_state == TCP_ESTABLISHED; }
static bool is_not_syn_sent(void) { return g_state != TCP_SYN_SENT; }

bool tcp_connect(ipv4_addr_t dst_ip, uint16_t dst_port) {
    // Reset state
    g_state       = TCP_CLOSED;
    g_remote_ip   = dst_ip;
    g_remote_port = dst_port;
    g_local_port  = g_ephemeral++;
    if (g_ephemeral > 60000) g_ephemeral = 49200;
    g_seq         = 0x12345678; // fixed ISN fine for baremetal
    g_ack         = 0;
    g_got_fin     = false;
    g_recv_head   = 0;
    g_recv_tail   = 0;
    g_recv_count  = 0;
    g_retx_len    = 0;
    g_retx_tries  = 0;
    g_remote_win  = 1460;
    ooo_reset();

    // Send SYN, save for retransmit
    g_state = TCP_SYN_SENT;
    g_retx_seq         = g_seq;
    g_retx_len         = 0; // SYN has no payload
    g_retx_deadline_ms = pit_ms() + 1000;
    g_retx_tries       = 0;

    bool ok = tcp_send_segment(TCP_SYN, g_seq, NULL, 0);
    if (!ok) {
        // ARP not resolved yet — poll until it is then retry
        uint32_t deadline = pit_ms() + TCP_CONNECT_TIMEOUT_MS;
        while (!ok && pit_ms() < deadline) {
            extern void net_poll(void);
            net_poll();
            if (sched_app_running()) sched_yield();
            ok = tcp_send_segment(TCP_SYN, g_seq, NULL, 0);
        }
        if (!ok) { g_state = TCP_CLOSED; return false; }
    }
    // SYN consumes one seq number
    g_seq++;

    // Wait for SYN-ACK
    poll_until(is_not_syn_sent, TCP_CONNECT_TIMEOUT_MS);

    return g_state == TCP_ESTABLISHED;
}

bool tcp_is_connected(void) {
    return g_state == TCP_ESTABLISHED || g_state == TCP_CLOSE_WAIT;
}

int tcp_send(const void *data, uint16_t len) {
    if (!tcp_is_connected()) return -1;
    if (len == 0) return 0;

    // Chunk into MSS-sized pieces (1460 bytes)
    const uint8_t *p = (const uint8_t *)data;
    uint16_t remaining = len;
    int total_sent = 0;

    while (remaining > 0) {
        if (!tcp_is_connected()) return total_sent > 0 ? total_sent : -1;

        uint16_t chunk = remaining > 1460 ? 1460 : remaining;

        // Save for retransmit
        for (uint16_t i = 0; i < chunk; i++) g_retx_buf[i] = p[i];
        g_retx_len         = chunk;
        g_retx_seq         = g_seq;
        g_retx_deadline_ms = pit_ms() + 2000;
        g_retx_tries       = 0;

        bool ok = tcp_send_segment(TCP_PSH | TCP_ACK, g_seq, p, chunk);
        if (!ok) {
            // Wait for ARP or window
            uint32_t deadline = pit_ms() + 2000;
            while (!ok && pit_ms() < deadline) {
                extern void net_poll(void);
                net_poll();
                if (sched_app_running()) sched_yield();
                ok = tcp_send_segment(TCP_PSH | TCP_ACK, g_seq, p, chunk);
            }
            if (!ok) return total_sent > 0 ? total_sent : -1;
        }
        g_seq += chunk;

        // Wait for ACK before sending next chunk
        uint32_t deadline = pit_ms() + 3000;
        while (g_retx_len > 0 && tcp_is_connected() && pit_ms() < deadline) {
            extern void net_poll(void);
            net_poll();
            if (sched_app_running()) sched_yield();
        }

        p            += chunk;
        remaining    -= chunk;
        total_sent   += chunk;
    }

    return total_sent;
}

int tcp_recv(void *buf, uint16_t len) {
    if (g_state == TCP_CLOSED) return -1;

    // Wait for data or FIN
    uint32_t deadline = pit_ms() + TCP_DATA_TIMEOUT_MS;
    while (g_recv_count == 0 && !g_got_fin && g_state != TCP_CLOSED) {
        if (pit_ms() > deadline) return -1;
        extern void net_poll(void);
        net_poll();
        if (sched_app_running()) sched_yield();
    }

    if (g_recv_count == 0) return 0; // clean close

    uint8_t *out = (uint8_t *)buf;
    uint16_t n = (uint16_t)(g_recv_count < len ? g_recv_count : len);
    for (uint16_t i = 0; i < n; i++) {
        out[i] = g_recv_buf[g_recv_tail];
        g_recv_tail = (g_recv_tail + 1) % TCP_RECV_BUF_SIZE;
        g_recv_count--;
    }
    return (int)n;
}

int tcp_recv_all(void *buf, uint32_t max_len) {
    uint8_t *out = (uint8_t *)buf;
    uint32_t total = 0;

    // One absolute deadline for the entire download — prevents each call to
    // tcp_recv() resetting a per-chunk 10-second timer indefinitely. The
    // server *will* send a FIN when it's done (HTTP/1.0 Connection: close),
    // so we should always hit the g_got_fin/TCP_CLOSED break first on a
    // healthy connection. This deadline is the backstop for a stalled server.
    uint32_t deadline = pit_ms() + TCP_DATA_TIMEOUT_MS;

    while (total < max_len) {
        // Done when remote closed its side and we've drained the buffer.
        if ((g_got_fin || g_state == TCP_CLOSED) && g_recv_count == 0) break;

        // Safety net: abort if the overall deadline has passed with no
        // progress (avoids an infinite spin if FIN never arrives).
        if (pit_ms() > deadline) break;

        uint16_t chunk = (uint16_t)(max_len - total > 4096 ? 4096 : max_len - total);
        int n = tcp_recv(out + total, chunk);
        if (n < 0) break;          // error or hard timeout in tcp_recv
        if (n == 0) {
            // tcp_recv returns 0 on clean close (g_got_fin or TCP_CLOSED
            // with empty buffer) — stop regardless of g_got_fin state so we
            // don't spin forever when the server sends RST instead of FIN.
            if (g_got_fin || g_state == TCP_CLOSED) break;
            // n==0 but connection still open: shouldn't happen after the
            // wait loop in tcp_recv, but guard against it anyway.
            continue;
        }
        total += (uint32_t)n;
        // Got fresh data — extend deadline so a slow server isn't killed
        // mid-transfer; only a true stall (no data at all) hits the limit.
        deadline = pit_ms() + TCP_DATA_TIMEOUT_MS;
    }

    return (int)total;
}

void tcp_close(void) {
    if (g_state == TCP_CLOSED) return;

    if (g_state == TCP_ESTABLISHED) {
        g_state = TCP_FIN_WAIT1;
        tcp_send_segment(TCP_FIN | TCP_ACK, g_seq, NULL, 0);
        g_seq++;

        // Wait briefly for FIN-ACK
        uint32_t deadline = pit_ms() + 3000;
        while (g_state != TCP_TIME_WAIT && g_state != TCP_CLOSED
               && pit_ms() < deadline) {
            extern void net_poll(void);
            net_poll();
            if (sched_app_running()) sched_yield();
        }
    } else if (g_state == TCP_CLOSE_WAIT) {
        g_state = TCP_LAST_ACK;
        tcp_send_segment(TCP_FIN | TCP_ACK, g_seq, NULL, 0);
        g_seq++;
        uint32_t deadline = pit_ms() + 2000;
        while (g_state != TCP_CLOSED && pit_ms() < deadline) {
            extern void net_poll(void);
            net_poll();
            if (sched_app_running()) sched_yield();
        }
    }

    g_state = TCP_CLOSED;
}
