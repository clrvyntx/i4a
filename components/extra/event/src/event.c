#include "event.h"
#include "esp_log.h"

static const char *TAG = "RoutingEventHandlers";

// Handle peer connected event
esp_err_t on_peer_connected(void *ctx, uint32_t network, uint32_t mask) {
    (void)ctx;  // suppress unused warning if ctx not used
    ESP_LOGI(TAG, "Peer connected: Network %u, Mask %u", network, mask);
    // Additional handling logic
    return ESP_OK;
}

// Handle peer message event
esp_err_t on_peer_message(void *ctx, const uint8_t *msg, uint16_t len) {
    (void)ctx;
    if (msg == NULL || len == 0) {
        ESP_LOGE(TAG, "Invalid message data");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Received message of length %u", len);
    ESP_LOG_BUFFER_HEX(TAG, msg, len);  // safer than assuming null-terminated string
    // Additional handling logic
    return ESP_OK;
}

// Handle peer lost event
esp_err_t on_peer_lost(void *ctx, uint32_t network, uint32_t mask) {
    (void)ctx;
    ESP_LOGI(TAG, "Peer lost: Network %u, Mask %u", network, mask);
    // Additional handling logic
    return ESP_OK;
}

// Handle sibling message event
esp_err_t on_sibling_message(void *ctx, const uint8_t *msg, uint16_t len) {
    (void)ctx;
    if (msg == NULL || len == 0) {
        ESP_LOGE(TAG, "Invalid sibling message data");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Sibling message received of length %u", len);
    ESP_LOG_BUFFER_HEX(TAG, msg, len);
    // Additional handling logic
    return ESP_OK;
}
