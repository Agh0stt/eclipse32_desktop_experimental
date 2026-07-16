// =============================================================================
// Eclipse32 - TCP (RFC 793, client-side only)
// Single connection at a time. No listen/accept. No out-of-order reassembly.
// Enough for HTTP GET.
// =============================================================================
#pragma once
#include "net.h"

// TCP states (client-side only)
typedef enum {
    TCP_CLOSED = 0,
    TCP_SYN_SENT,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_TIME_WAIT,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
} tcp_state_t;

// Receive buffer size
#define TCP_RECV_BUF_SIZE   (32 * 1024)   // 32 KB
#define TCP_SEND_BUF_SIZE   (4  * 1024)   // 4 KB
#define TCP_CONNECT_TIMEOUT_MS  5000
#define TCP_DATA_TIMEOUT_MS    10000
#define TCP_MAX_RETRIES          5

// Connect to a remote host. Blocking — spins on net_poll()+sched_yield()
// until ESTABLISHED or timeout. Returns true on success.
bool tcp_connect(ipv4_addr_t dst_ip, uint16_t dst_port);

// Send data on the established connection. Blocks until all data is sent
// or the connection drops. Returns bytes sent, <0 on error.
int tcp_send(const void *data, uint16_t len);

// Read up to `len` bytes from receive buffer. Blocks until data arrives
// or connection closes. Returns bytes read, 0 on clean close, <0 on error.
int tcp_recv(void *buf, uint16_t len);

// Read until connection closes (FIN received). Returns total bytes read into
// buf (up to max_len). Good for HTTP/1.0 where server closes after response.
int tcp_recv_all(void *buf, uint32_t max_len);

// Close the connection (sends FIN, waits for FIN-ACK).
void tcp_close(void);

// True if connection is up and usable.
bool tcp_is_connected(void);

// Called by ipv4_handle_packet for IPV4_PROTO_TCP frames.
void tcp_handle_packet(ipv4_addr_t src_ip, const uint8_t *data, uint16_t len);

// Call from net_poll to handle retransmits / timeouts.
void tcp_tick(void);
