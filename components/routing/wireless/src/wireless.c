#include "wireless/wireless.h"
#include "node.h"

void wl_register_peer_callbacks(wireless_t *wl, wireless_callbacks_t callbacks, void *context) {
    wl->callbacks = callbacks;
    wl->context = context;
}

bool wl_send_peer_message(wireless_t *wl, const void *msg, uint16_t len){
    return node_send_wireless_message(msg, len);
}

void wl_enable_ap_mode(wireless_t *wl, uint32_t network, uint32_t mask){
    return node_set_ap_network(network, mask);
}
