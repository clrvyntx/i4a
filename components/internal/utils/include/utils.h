#ifndef __UTILS_H
#define __UTILS_H

#include "lwip/netif.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "lwip/udp.h"
#include "lwip/ip6.h"
#include "lwip/inet_chksum.h"

void print_route_table(void);
#endif
