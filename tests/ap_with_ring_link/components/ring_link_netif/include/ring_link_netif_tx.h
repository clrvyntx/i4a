#pragma once

#include "ring_link_netif_common.h"

#ifdef __cplusplus
extern "C" {
#endif


ESP_EVENT_DECLARE_BASE(RING_LINK_TX_EVENT);

esp_err_t ring_link_tx_netif_init(void);

esp_netif_t *get_ring_link_tx_netif(void);

err_t ring_link_tx_netstack_init_fn(struct netif *netif);

esp_netif_recv_ret_t ring_link_tx_netstack_input_fn(void *h, void *buffer, size_t len, void* l2_buff);

esp_ip4_addr_t get_spi_tx_ip_interface_address(void);

#ifdef __cplusplus
}
#endif
