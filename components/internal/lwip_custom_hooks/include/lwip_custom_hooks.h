#pragma once

#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
    #endif

    struct netif* custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest);

    #ifdef __cplusplus
}
#endif
