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
struct pbuf * udp_create_test_packet(u16_t length, u16_t dst_port, u16_t src_port, u32_t dst_addr, u32_t src_addr);
struct pbuf * udp_create_test_packet_(u16_t length, u16_t dst_port, u16_t src_port, u32_t dst_addr, u32_t src_addr, u8_t * test_data);
struct pbuf *udp_create_test_packet_ipv6(u16_t length, u16_t dst_port, u16_t src_port, const ip6_addr_t *dst_addr, const ip6_addr_t *src_addr);
struct pbuf *udp_create_test_packet_ipv6_(u16_t length, u16_t dst_port, u16_t src_port, const ip6_addr_t *dst_addr, const ip6_addr_t *src_addr, u8_t test_data);

#endif
