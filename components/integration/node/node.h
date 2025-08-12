#ifndef _NODE_H_
#define _NODE_H_

#include "device.h"
#include "config.h"

typedef struct node {
  DevicePtr node_device_ptr;
  config_orientation_t node_device_orientation;
  bool node_device_is_center_root;
} node_t;

DevicePtr node_setup();
void node_set_as_ap(uint32_t network, uint32_t mask);
void node_set_as_sta();

#endif
