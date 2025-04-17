#include "wireless/wireless.h"

void wl_register_peer_callbacks(wireless_t *wl, wireless_callbacks_t callbacks, void *context) {
    wl->callbacks = callbacks;
    wl->context = context;
}

void wl_enable_ap_mode(wireless_t *wl, uint32_t network, uint32_t mask) {
    ip4_addr_t ip, gw, netmask;

    ip.addr = htonl(network);
    gw.addr = htonl(network + 1);
    netmask.addr = htonl(mask);

    const char *ip_str = ip4addr_ntoa(&ip);
    const char *gw_str = ip4addr_ntoa(&gw);
    const char *mask_str = ip4addr_ntoa(&netmask);

    device_set_network_ap(wl->device, ip_str, gw_str, mask_str);
}
