#pragma once

#include "lwip/netif.h"

/* lwIP initializer for local-only configuration and orientation APs. */
err_t configuration_ap_filter_init(struct netif *netif);
