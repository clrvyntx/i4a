#pragma once

#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

// SPI interfaces need to bounce back packets if not for them when forwarding
#undef IP_FORWARD_ALLOW_TX_ON_RX_NETIF
#define IP_FORWARD_ALLOW_TX_ON_RX_NETIF 1

#undef LWIP_HOOK_IP4_ROUTE_SRC
#define LWIP_HOOK_IP4_ROUTE_SRC custom_ip4_route_src_hook

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest);

#ifdef __cplusplus
}
#endif

