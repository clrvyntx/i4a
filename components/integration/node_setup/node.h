#ifndef _NODE__H_
#define _NODE_H_

#include "config.h"
#include "device.h"

typedef struct node {
    Device node_device;
    DevicePtr node_device_ptr;
    config_orientation_t node_device_orientation;
    bool node_center_is_root;
} node_t;

void node_setup();
void node_set_as_ap(uint32_t network, uint32_t mask);
void node_set_as_sta();

#endif
