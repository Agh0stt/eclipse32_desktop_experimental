// =============================================================================
// Eclipse32 - ARP (RFC 826)
// =============================================================================
#include "arp.h"
#include "eth.h"
#include "../drivers/net/rtl8139.h"
#include "../arch/x86/pit.h"

#define ARP_HW_ETHERNET   1
#define ARP_OP_REQUEST    1
#define ARP_OP_REPLY      2

#pragma pack(push, 1)
typedef struct {
    uint16_t hw_type;        // big-endian, ARP_HW_ETHERNET
    uint16_t proto_type;     // big-endian, ETHERTYPE_IPV4
    uint8_t  hw_len;         // 6
    uint8_t  proto_len;      // 4
    uint16_t opcode;         // big-endian, ARP_OP_REQUEST/REPLY
    uint8_t  sender_mac[MAC_ADDR_LEN];
    uint8_t  sender_ip[IPV4_ADDR_LEN];
    uint8_t  target_mac[MAC_ADDR_LEN];
    uint8_t  target_ip[IPV4_ADDR_LEN];
} arp_packet_t;
#pragma pack(pop)

#define ARP_CACHE_SIZE     16
#define ARP_RETRY_MS       1000
#define ARP_MAX_RETRIES    5

typedef struct {
    bool        used;
    ipv4_addr_t ip;
    mac_addr_t  mac;
    bool        resolved;       // false while we're still waiting on a reply
    uint32_t    last_request_ms;
    uint8_t     retries;
} arp_cache_entry_t;

static arp_cache_entry_t cache[ARP_CACHE_SIZE];

extern mac_addr_t  net_get_mac(void);
extern ipv4_addr_t net_get_ip(void);

static arp_cache_entry_t *cache_find(ipv4_addr_t ip) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (cache[i].used && ipv4_eq(cache[i].ip, ip)) return &cache[i];
    }
    return NULL;
}

static arp_cache_entry_t *cache_find_or_create(ipv4_addr_t ip) {
    arp_cache_entry_t *e = cache_find(ip);
    if (e) return e;

    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!cache[i].used) {
            cache[i].used = true;
            cache[i].ip = ip;
            cache[i].resolved = false;
            cache[i].retries = 0;
            cache[i].last_request_ms = 0;
            return &cache[i];
        }
    }
    // Cache full: evict slot 0 (simplest possible policy; fine for a single
    // gateway + a couple hosts, which is all this stack talks to right now).
    cache[0].used = true;
    cache[0].ip = ip;
    cache[0].resolved = false;
    cache[0].retries = 0;
    cache[0].last_request_ms = 0;
    return &cache[0];
}

static void send_arp_request(ipv4_addr_t target_ip) {
    arp_packet_t pkt;
    pkt.hw_type    = net_htons(ARP_HW_ETHERNET);
    pkt.proto_type = net_htons(ETHERTYPE_IPV4);
    pkt.hw_len     = MAC_ADDR_LEN;
    pkt.proto_len  = IPV4_ADDR_LEN;
    pkt.opcode     = net_htons(ARP_OP_REQUEST);

    mac_addr_t my_mac = net_get_mac();
    ipv4_addr_t my_ip = net_get_ip();
    for (int i = 0; i < MAC_ADDR_LEN; i++) {
        pkt.sender_mac[i] = my_mac.b[i];
        pkt.target_mac[i] = 0; // unknown — that's what we're asking for
    }
    for (int i = 0; i < IPV4_ADDR_LEN; i++) {
        pkt.sender_ip[i] = my_ip.b[i];
        pkt.target_ip[i] = target_ip.b[i];
    }

    eth_send(mac_broadcast(), ETHERTYPE_ARP, &pkt, sizeof(pkt));
}

bool arp_resolve(ipv4_addr_t ip, mac_addr_t *out_mac) {
    arp_cache_entry_t *e = cache_find_or_create(ip);

    if (e->resolved) {
        if (out_mac) *out_mac = e->mac;
        return true;
    }

    // Not resolved yet — kick off (or this is the first call for this IP)
    // a request if we haven't sent one recently. arp_tick() handles retries
    // for requests already in flight, so here we only fire on the very first
    // ask (last_request_ms == 0 and retries == 0).
    if (e->retries == 0 && e->last_request_ms == 0) {
        send_arp_request(ip);
        e->last_request_ms = pit_ms();
        e->retries = 1;
    }

    return false;
}

void arp_cache_insert(ipv4_addr_t ip, mac_addr_t mac) {
    arp_cache_entry_t *e = cache_find_or_create(ip);
    e->mac = mac;
    e->resolved = true;
}

void arp_handle_packet(const uint8_t *data, uint16_t len, mac_addr_t src_mac) {
    (void)src_mac;
    if (len < sizeof(arp_packet_t)) return;

    const arp_packet_t *pkt = (const arp_packet_t *)data;
    if (net_ntohs(pkt->hw_type) != ARP_HW_ETHERNET) return;
    if (net_ntohs(pkt->proto_type) != ETHERTYPE_IPV4) return;

    ipv4_addr_t sender_ip = ipv4_make(pkt->sender_ip[0], pkt->sender_ip[1],
                                      pkt->sender_ip[2], pkt->sender_ip[3]);
    mac_addr_t sender_mac;
    for (int i = 0; i < MAC_ADDR_LEN; i++) sender_mac.b[i] = pkt->sender_mac[i];

    uint16_t opcode = net_ntohs(pkt->opcode);

    if (opcode == ARP_OP_REQUEST) {
        ipv4_addr_t target_ip = ipv4_make(pkt->target_ip[0], pkt->target_ip[1],
                                           pkt->target_ip[2], pkt->target_ip[3]);
        ipv4_addr_t my_ip = net_get_ip();
        if (ipv4_eq(target_ip, my_ip)) {
            // Someone's asking for us — reply directly to them.
            arp_packet_t reply;
            reply.hw_type    = net_htons(ARP_HW_ETHERNET);
            reply.proto_type = net_htons(ETHERTYPE_IPV4);
            reply.hw_len     = MAC_ADDR_LEN;
            reply.proto_len  = IPV4_ADDR_LEN;
            reply.opcode     = net_htons(ARP_OP_REPLY);

            mac_addr_t my_mac = net_get_mac();
            for (int i = 0; i < MAC_ADDR_LEN; i++) {
                reply.sender_mac[i] = my_mac.b[i];
                reply.target_mac[i] = sender_mac.b[i];
            }
            for (int i = 0; i < IPV4_ADDR_LEN; i++) {
                reply.sender_ip[i] = my_ip.b[i];
                reply.target_ip[i] = sender_ip.b[i];
            }
            eth_send(sender_mac, ETHERTYPE_ARP, &reply, sizeof(reply));
        }
    }

    // Whether it was a request or reply, learn the sender's mapping —
    // standard ARP cache behavior ("opportunistic learning").
    arp_cache_insert(sender_ip, sender_mac);
}

void arp_tick(void) {
    uint32_t now = pit_ms();
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        arp_cache_entry_t *e = &cache[i];
        if (!e->used || e->resolved) continue;
        if (e->retries == 0 || e->retries > ARP_MAX_RETRIES) continue;
        if ((now - e->last_request_ms) < ARP_RETRY_MS) continue;

        send_arp_request(e->ip);
        e->last_request_ms = now;
        e->retries++;
    }
}
