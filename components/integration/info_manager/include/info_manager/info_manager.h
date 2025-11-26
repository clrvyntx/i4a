#ifndef _INFO_MANAGER_H_
#define _INFO_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>

#include "ring_share/ring_share.h"

#define MAX_ORIENTATIONS 5
#define UUID_LENGTH 13

typedef struct __attribute__((packed)) {
    char uuid[UUID_LENGTH];
    uint8_t orientation;
    uint32_t subnet;
    uint32_t mask;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint8_t is_root;
    int8_t rssi;
} im_ring_packet_t;

typedef struct {
    im_ring_packet_t ring[MAX_ORIENTATIONS];
    ring_share_t *rs;
} im_manager_t;

void im_init(ring_share_t *rs);
void im_scheduler_start(void);
void im_http_client_start(void);
bool im_broadcast_info(void);
const im_ring_packet_t *im_get_ring_info(void);

#endif  // _INFO_MANAGER_H_
