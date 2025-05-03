#include "event.h"

// Log tag for the event handlers
static const char *TAG = "EventHandlers";

// Handle peer connected event
esp_err_t handle_peer_connected(void *data) {
    peer_connected_data_t *connected_data = (peer_connected_data_t *)data;
    if (connected_data == NULL) {
        ESP_LOGE(TAG, "Null data for peer connected event");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Peer connected: Network %u, Mask %u", connected_data->network, connected_data->mask);
    // Additional handling logic
    return ESP_OK;
}

// Handle peer message event
esp_err_t handle_peer_message(void *data) {
    peer_message_data_t *message_data = (peer_message_data_t *)data;
    if (message_data == NULL) {
        ESP_LOGE(TAG, "Null data for peer message event");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Received message: %s", message_data->msg);
    // Additional handling logic
    return ESP_OK;
}

// Handle peer lost event
esp_err_t handle_peer_lost(void *data) {
    peer_lost_data_t *lost_data = (peer_lost_data_t *)data;
    if (lost_data == NULL) {
        ESP_LOGE(TAG, "Null data for peer lost event");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Peer lost: Network %u, Mask %u", lost_data->network, lost_data->mask);
    // Additional handling logic
    return ESP_OK;
}

// Handle sibling message event
esp_err_t handle_sibling_message(void *data) {
    sibling_message_data_t *sibling_data = (sibling_message_data_t *)data;
    if (sibling_data == NULL) {
        ESP_LOGE(TAG, "Null data for sibling message event");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Sibling message received: %s", sibling_data->msg);
    // Additional handling logic
    return ESP_OK;
}
