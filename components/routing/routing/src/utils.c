#include "utils.h"

#include <stdint.h>

#include "routing/routing.h"

uint32_t mask_size(uint32_t n) {
    uint32_t c = 0;

    for (; n; ++c) {
        n <<= 1;
    }

    return c;
}

network_t get_node_subnet(const network_t *node_network, orientation_t orientation) {
    uint32_t assigned_block = (uint32_t)orientation;
    uint32_t prefix_len = mask_size(node_network->mask) + 3;
    uint32_t new_mask = ((1 << prefix_len) - 1) << (32 - prefix_len);
    uint32_t new_network = node_network->addr | (assigned_block << (32 - mask_size(new_mask)));
    return (network_t){
        .addr = new_network,
        .mask = new_mask,
    };
}

void add_global_route(routing_t *self, const network_t *route, orientation_t output) {
    WITH_LOCK(&self->node_state.m_lock, {
        routing_table_add(&self->node_state.routing_table, route->addr, route->mask, output);
    });
    ss_refresh(self->deps.ss, RS_ROUTING);
}

void remove_routes_by_output(routing_t *self, orientation_t output) {
    WITH_LOCK(&self->node_state.m_lock, { routing_table_remove_by_output(&self->node_state.routing_table, output); });
    ss_refresh(self->deps.ss, RS_ROUTING);
}

network_t *find_free_spot(network_t networks[], size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (networks[i].addr == 0)
            return &networks[i];
    }

    return NULL;
}