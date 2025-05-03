#ifndef _EVENT_H_
#define _EVENT_H_

#include "esp_err.h"

// Peer connected handler
esp_err_t on_peer_connected(void *ctx, uint32_t network, uint32_t mask);

// Peer message handler
esp_err_t on_peer_message(void *ctx, const uint8_t *msg, uint16_t len);

// Peer lost handler
esp_err_t on_peer_lost(void *ctx, uint32_t network, uint32_t mask);

// Sibling message handler
esp_err_t on_sibling_message(void *ctx, const uint8_t *msg, uint16_t len);

#endif  // _EVENT_H_
