#ifndef _CHANNEL_MANAGER_H_
#define _CHANNEL_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>

#include "routing_config/routing_config.h"  // for orientation_t
#include "ring_share/ring_share.h"

typedef struct channel_manager {
    ring_share_t *rs;
    uint8_t suggested_channel;
    orientation_t orientation;
} channel_manager_t;

void cm_init(ring_share_t *rs, orientation_t orientation);
void channel_provision(uint8_t connected_channel);
uint8_t cm_get_suggested_channel(void);

#endif  // _CHANNEL_MANAGER_H_
