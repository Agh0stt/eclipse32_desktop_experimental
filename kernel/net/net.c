// =============================================================================
// Eclipse32 - Network stack glue
// Holds our local MAC/IP/gateway config and drives the receive poll loop.
// =============================================================================
#include "net.h"
#include "eth.h"
#include "arp.h"
#include "../drivers/net/rtl8139.h"

static mac_addr_t  g_my_mac;
static ipv4_addr_t g_my_ip;
static ipv4_addr_t g_gateway_ip;
static ipv4_addr_t g_dns_server_ip;
static bool         g_net_ready = false;

void net_init(mac_addr_t my_mac, ipv4_addr_t my_ip, ipv4_addr_t gateway_ip, ipv4_addr_t dns_server_ip) {
    g_my_mac = my_mac;
    g_my_ip = my_ip;
    g_gateway_ip = gateway_ip;
    g_dns_server_ip = dns_server_ip;
    g_net_ready = true;
}

mac_addr_t  net_get_mac(void)        { return g_my_mac; }
ipv4_addr_t net_get_ip(void)         { return g_my_ip; }
ipv4_addr_t net_get_gateway(void)    { return g_gateway_ip; }
ipv4_addr_t net_get_dns_server(void) { return g_dns_server_ip; }

void net_poll(void) {
    if (!g_net_ready) return;
    if (!rtl8139_present()) return;

    rtl8139_poll(eth_handle_frame);
    arp_tick();
}
