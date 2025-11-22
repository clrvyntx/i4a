#pragma once

#include "lwip/netif.h"
#include "lwip/opt.h"

#ifdef __cplusplus
extern "C" {
#endif

#undef PBUF_POOL_BUFSIZE
#define PBUF_POOL_BUFSIZE 1700

#undef PBUF_POOL_SIZE
#define PBUF_POOL_SIZE 50

#undef LWIP_HOOK_IP4_ROUTE_SRC
#define LWIP_HOOK_IP4_ROUTE_SRC custom_ip4_route_src_hook

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest);

#ifdef __cplusplus
}
#endif
