#include <stdlib.h>
#include "callback.h"
#include "event.h"
#include "control.h"
#include "esp_log.h"

static const char *TAG = "ControlCallbacks";

void on_peer_connected(void *context, uint32_t network, uint32_t mask) {
    control_t *control = (control_t *)context;
    if (!control || !control->routing_event_queue) {
        ESP_LOGE(TAG, "Invalid context or routing queue in on_peer_connected");
        return;
    }

    peer_connected_data_t *data = malloc(sizeof(peer_connected_data_t));
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate memory for peer connected event");
        return;
    }

    data->network = network;
    data->mask = mask;

    routing_event_t event = {
        .event_type = ROUTING_EVENT_PEER_CONNECTED,
        .data = data
    };

    if (xQueueSend(control->routing_event_queue, &event, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to queue peer connected event");
        free(data);
    }
}

void on_peer_lost(void *context, uint32_t network, uint32_t mask) {
    control_t *control = (control_t *)context;
    if (!control || !control->routing_event_queue) {
        ESP_LOGE(TAG, "Invalid context or routing queue in on_peer_lost");
        return;
    }

    peer_lost_data_t *data = malloc(sizeof(peer_lost_data_t));
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate memory for peer lost event");
        return;
    }

    data->network = network;
    data->mask = mask;

    routing_event_t event = {
        .event_type = ROUTING_EVENT_PEER_LOST,
        .data = data
    };

    if (xQueueSend(control->routing_event_queue, &event, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to queue peer lost event");
        free(data);
    }
}

void on_peer_message(void *context, const uint8_t *msg, uint16_t len) {
    control_t *control = (control_t *)context;
    if (!control || !control->routing_event_queue) {
        ESP_LOGE(TAG, "Invalid context or routing queue in on_peer_message");
        return;
    }

    peer_message_data_t *data = malloc(sizeof(peer_message_data_t));
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate memory for peer message event");
        return;
    }

    data->msg = malloc(len);
    if (!data->msg) {
        ESP_LOGE(TAG, "Failed to allocate memory for peer message content");
        free(data);
        return;
    }

    memcpy(data->msg, msg, len);
    data->len = len;

    routing_event_t event = {
        .event_type = ROUTING_EVENT_PEER_MESSAGE,
        .data = data
    };

    if (xQueueSend(control->routing_event_queue, &event, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to queue peer message event");
        free(data->msg);
        free(data);
    }
}

void on_sibling_message(void *context, const uint8_t *msg, uint16_t len) {
    control_t *control = (control_t *)context;
    if (!control || !control->routing_event_queue) {
        ESP_LOGE(TAG, "Invalid context or routing queue in on_sibling_message");
        return;
    }

    sibling_message_data_t *data = malloc(sizeof(sibling_message_data_t));
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate memory for sibling message event");
        return;
    }

    data->msg = malloc(len);
    if (!data->msg) {
        ESP_LOGE(TAG, "Failed to allocate memory for sibling message content");
        free(data);
        return;
    }

    memcpy(data->msg, msg, len);
    data->len = len;

    routing_event_t event = {
        .event_type = ROUTING_EVENT_SIBLING_MESSAGE,
        .data = data
    };

    if (xQueueSend(control->routing_event_queue, &event, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to queue sibling message event");
        free(data->msg);
        free(data);
    }
}
