#ifndef _EVENT_H_
#define _EVENT_H_

#include "esp_err.h"

// Event data structure for Peer Connected event
typedef struct peer_connected_data_t {
    uint32_t network;
    uint32_t mask;
} peer_connected_data_t;

// Event data structure for Peer Message event
typedef struct peer_message_data_t {
    uint8_t *msg;
    uint16_t len;
} peer_message_data_t;

// Event data structure for Peer Lost event
typedef struct peer_lost_data_t {
    uint32_t network;
    uint32_t mask;
} peer_lost_data_t;

// Event data structure for Sibling Message event
typedef struct sibling_message_data_t {
    uint8_t *msg;
    uint16_t len;
} sibling_message_data_t;

#endif  // _EVENT_H_
