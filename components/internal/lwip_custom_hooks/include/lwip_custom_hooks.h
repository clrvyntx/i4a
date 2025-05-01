#pragma once

#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

#undef LWIP_HOOK_IP4_ROUTE_SRC
#define LWIP_HOOK_IP4_ROUTE_SRC custom_ip4_route_src_hook

struct netif* custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest);
bool get_higher_priority_mask(ip4_addr_t* mask1, ip4_addr_t* mask2);

#ifdef __cplusplus
}
#endif