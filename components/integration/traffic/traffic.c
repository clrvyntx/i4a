#include "esp_event.h"
#include "esp_log.h"
#include "traffic.h"

static const char *TAG = "node_traffic";

static uint64_t tx_bytes = 0;
static uint64_t rx_bytes = 0;

static void tx_rx_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ip_event_tx_rx_t *ev = (ip_event_tx_rx_t *)event_data;
    if (ev->dir == ESP_NETIF_TX) {
        tx_bytes += ev->len;
    } else if (ev->dir == ESP_NETIF_RX) {
        rx_bytes += ev->len;
    }
}

void node_traffic_init(void) {
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_TX_RX, tx_rx_event_handler, NULL));
    ESP_LOGI(TAG, "Traffic monitoring initialized");
}

void node_traffic_start(esp_netif_t *netif) {
    ESP_LOGI(TAG, "Starting traffic monitoring on netif %p", netif);
    esp_netif_tx_rx_event_enable(netif);
}

void node_traffic_stop(esp_netif_t *netif) {
    ESP_LOGI(TAG, "Stopping traffic monitoring on netif %p", netif);
    esp_netif_tx_rx_event_disable(netif);
}

uint64_t node_traffic_get_tx_count(void) {
    return tx_bytes;
}

uint64_t node_traffic_get_rx_count(void) {
    return rx_bytes;
}

void node_traffic_reset_counters(void) {
    tx_bytes = 0;
    rx_bytes = 0;
}
