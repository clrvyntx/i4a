#ifndef _ROUTING_UTILS_H_
#define _ROUTING_UTILS_H_

#include <stdint.h>

#include "routing/routing.h"

uint32_t mask_size(uint32_t n);
network_t get_node_subnet(const network_t *node_network, orientation_t orientation);

void add_global_route(routing_t *self, const network_t *route, orientation_t output);
void remove_routes_by_output(routing_t *self, orientation_t output);

network_t *find_free_spot(network_t networks[], size_t length);

#endif  // _ROUTING_UTILS_H_
