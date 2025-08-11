#ifndef _NODE_H_
#define _NODE_H_

#define NODE_UUID_LEN 7

#include "config.h"
#include "device.h"

typedef struct node {
    Device node_device;
    DevicePtr node_device_ptr;
    config_orientation_t node_device_orientation;
    bool node_center_is_root;
    char node_uuid[NODE_UUID_LEN];
} node_t;

node_t *node_setup();
void node_set_as_ap(node_t *current_node_ptr, uint32_t network, uint32_t mask);
void node_set_as_sta(node_t *current_node_ptr);

#endif
