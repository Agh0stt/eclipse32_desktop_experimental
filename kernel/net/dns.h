// =============================================================================
// Eclipse32 - DNS client (RFC 1035, minimal: A record queries only)
// =============================================================================
#pragma once
#include "net.h"

// Resolve `hostname` (e.g. "claude.ai") to an IPv4 address via the
// configured DNS server (the gateway, by default — matches how most simple
// setups including QEMU user-mode networking work, since the gateway also
// answers DNS at 10.0.2.2). Non-blocking: returns false immediately if a
// query was just sent and we're still waiting, or if no query is in flight
// yet (call again after sending one — see dns_query()). For a synchronous,
// "just block until done or timeout" experience, use dns_resolve_blocking().
bool dns_query(const char *hostname);

// Call repeatedly (with net_poll() in between) after dns_query() until it
// returns true (resolved, *out_ip valid) or until your own timeout expires.
// Returns true exactly once per query, the moment a reply matching the
// in-flight query arrives.
bool dns_poll_result(ipv4_addr_t *out_ip);

// Convenience wrapper: sends the query and spin-polls (yielding to net_poll()
// only — no scheduler yield, callers in a cooperatively-scheduled context
// should prefer dns_query()+dns_poll_result() in their own loop so they can
// also call sched_yield()). Returns true if resolved within timeout_ms.
bool dns_resolve_blocking(const char *hostname, ipv4_addr_t *out_ip, uint32_t timeout_ms);

// Dispatch entry point — registered internally via udp_bind() on the
// ephemeral source port we use for outgoing queries.
