#include "wireless/wireless.h"

void wl_register_peer_callbacks(wireless_t *wl, wireless_callbacks_t callbacks, void *context) {
    wl->callbacks = callbacks;
    wl->context = context;
}
