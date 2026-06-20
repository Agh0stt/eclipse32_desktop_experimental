// =============================================================================
// Eclipse32 - Ethernet II framing
// =============================================================================
#include "eth.h"
#include "arp.h"
#include "ipv4.h"
#include "../drivers/net/rtl8139.h"

bool eth_send(mac_addr_t dst, uint16_t ethertype, const void *payload, uint16_t payload_len) {
    if (payload_len > ETH_MTU) return false;

    uint8_t frame[ETH_FRAME_MAX];
    eth_header_t *hdr = (eth_header_t *)frame;

    mac_addr_t my_mac = rtl8139_get_mac();
    for (int i = 0; i < MAC_ADDR_LEN; i++) {
        hdr->dst[i] = dst.b[i];
        hdr->src[i] = my_mac.b[i];
    }
    hdr->ethertype = net_htons(ethertype);

    for (uint16_t i = 0; i < payload_len; i++) {
        frame[ETH_HEADER_LEN + i] = ((const uint8_t *)payload)[i];
    }

    return rtl8139_send(frame, (uint16_t)(ETH_HEADER_LEN + payload_len));
}

void eth_handle_frame(const uint8_t *data, uint16_t len) {
    if (len < ETH_HEADER_LEN) return;

    const eth_header_t *hdr = (const eth_header_t *)data;
    uint16_t ethertype = net_ntohs(hdr->ethertype);

    const uint8_t *payload = data + ETH_HEADER_LEN;
    uint16_t payload_len = (uint16_t)(len - ETH_HEADER_LEN);

    mac_addr_t src_mac;
    for (int i = 0; i < MAC_ADDR_LEN; i++) src_mac.b[i] = hdr->src[i];

    if (ethertype == ETHERTYPE_ARP) {
        arp_handle_packet(payload, payload_len, src_mac);
    } else if (ethertype == ETHERTYPE_IPV4) {
        ipv4_handle_packet(payload, payload_len);
    }
    // Anything else (IPv6, VLAN tags, etc.) is silently ignored for now.
}
