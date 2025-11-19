#ifndef _ROUTING_HOOKS_H_
#define _ROUTING_HOOKS_H_

#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "routing/routing.h"

typedef enum routing_hook_type {
    ROUTING_HOOK_ROOT_CENTER,
    ROUTING_HOOK_FORWARDER,
    ROUTING_HOOK_HOME,
    ROUTING_HOOK_ROOT_FORWARDER,
    ROUTING_HOOK_COUNT
} routing_hook_type_t;

void node_set_routing_hook(routing_hook_type_t hook);
struct netif *node_do_routing(uint32_t src, uint32_t dst);
routing_t *node_get_rt_instance(void);

#endif // _ROUTING_HOOKS_H_
