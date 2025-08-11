#ifndef _NODE_H_
#define _NODE_H_

#include "ring_link.h"
#include "esp_check.h"
#include "device.h"
#include "config.h"

DevicePtr node_setup();
void node_set_as_ap(uint32_t network, uint32_t mask);
void node_set_as_sta();

#endif
