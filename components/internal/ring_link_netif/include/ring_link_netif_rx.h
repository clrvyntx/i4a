#pragma once

#include "ring_link_netif_common.h"

#ifdef __cplusplus
extern "C" {
#endif


ESP_EVENT_DECLARE_BASE(RING_LINK_RX_EVENT);

esp_err_t ring_link_rx_netif_init(void);

esp_err_t ring_link_rx_netif_receive(ring_link_payload_t *p);

err_t ring_link_rx_netstack_lwip_init_fn(struct netif *netif);

esp_netif_recv_ret_t ring_link_rx_netstack_lwip_input_fn(void *h, void *buffer, size_t len, void* l2_buff);

#ifdef __cplusplus
}
#endif
