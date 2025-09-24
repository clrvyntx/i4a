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

void cm_init(channel_manager_t *self, ring_share_t *rs, orientation_t orientation);

uint8_t cm_get_suggested_channel(channel_manager_t *self);

#endif  // _CHANNEL_MANAGER_H_
