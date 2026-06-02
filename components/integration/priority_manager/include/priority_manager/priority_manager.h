#ifndef _PRIORITY_MANAGER_H_
#define _PRIORITY_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>

#include "node.h"  // for node_device_orientation_t
#include "ring_share/ring_share.h"

typedef struct priority_manager {
    ring_share_t *rs;
    node_device_orientation_t orientation;
} priority_manager_t;

void pm_init(ring_share_t *rs, node_device_orientation_t orientation);
bool pm_provide_to_siblings(int8_t rssi);
uint8_t pm_get_suggested_priority(void);
void pm_reset_priority_list(void);

#endif  // _PRIORITY_MANAGER_H_
