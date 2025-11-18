#ifndef _TRAFFIC_H_
#define _TRAFFIC_H_

#include "esp_netif.h"

void node_traffic_init(void);         // registers event handler
void node_traffic_start(esp_netif_t *netif);        // enable TX/RX events for a certain netif
void node_traffic_stop(esp_netif_t *netif);         // disable TX/RX events for a certain netif

uint64_t node_traffic_get_tx_count(void);
uint64_t node_traffic_get_rx_count(void);
void node_traffic_reset_counters(void);

#endif  // _TRAFFIC_H_
