#include "control.h"
#include "esp_log.h"
#include "event.h"

#define CONTROL_TASK_STACK_SIZE     2048
#define CONTROL_TASK_PRIORITY       5
#define CONTROL_QUEUE_SIZE          20

static const char *TAG = "Control";

static void routing_event_task(void *param) {
    control_t *self = (control_t *)param;

    routing_event_t event;

    while (1) {
        if (xQueueReceive(self->routing_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            esp_err_t result = ESP_OK;

            switch (event.event_type) {
                case ROUTING_EVENT_PEER_CONNECTED:
                    result = handle_peer_connected(event.data);
                    break;
                case ROUTING_EVENT_PEER_MESSAGE:
                    result = handle_peer_message(event.data);
                    break;
                case ROUTING_EVENT_PEER_LOST:
                    result = handle_peer_lost(event.data);
                    break;
                case ROUTING_EVENT_SIBLING_MESSAGE:
                    result = handle_sibling_message(event.data);
                    break;
                default:
                    ESP_LOGE(TAG, "Unknown event type: %d", event.event_type);
                    result = ESP_ERR_INVALID_STATE;
                    break;
            }

            if (result != ESP_OK) {
                ESP_LOGE(TAG, "Event handler failed with error %d for event %d", result, event.event_type);
            }
        }
    }
}

control_t *control_create(void) {
    control_t *self = malloc(sizeof(control_t));
    if (!self) {
        ESP_LOGE(TAG, "Failed to allocate memory for control_t");
        return NULL;
    }

    // We assume control_init will initialize the fields,
    // so we don’t need to manually zero them here.

    return self;
}

esp_err_t control_init(control_t *self, wireless_callbacks_t wireless_callbacks, siblings_callback_t siblings_callback) {
    if (!self) {
        ESP_LOGE(TAG, "Null pointer passed to control_init");
        return ESP_ERR_INVALID_ARG;
    }

    self->wireless = malloc(sizeof(wireless_t));
    if (!self->wireless) {
        ESP_LOGE(TAG, "Failed to allocate wireless_t");
        return ESP_ERR_NO_MEM;
    }

    if (wireless_callbacks) {
        wl_register_peer_callbacks(self->wireless, wireless_callbacks, self);
    } else {
        ESP_LOGW(TAG, "Wireless callbacks are NULL, using default handling.");
    }

    self->siblings = malloc(sizeof(siblings_t));
    if (!self->siblings) {
        ESP_LOGE(TAG, "Failed to allocate siblings_t");
        free(self->wireless);
        return ESP_ERR_NO_MEM;
    }

    if (siblings_callback) {
        sb_register_callback(self->siblings, siblings_callback, self);
    } else {
        ESP_LOGW(TAG, "Siblings callbacks are NULL, using default handling.");
    }

    self->routing_event_queue = xQueueCreate(CONTROL_QUEUE_SIZE, sizeof(routing_event_t));
    if (!self->routing_event_queue) {
        ESP_LOGE(TAG, "Failed to create routing event queue");
        free(self->wireless);
        free(self->siblings);
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(routing_event_task, "RoutingEventTask", CONTROL_TASK_STACK_SIZE, self, CONTROL_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create routing event task");
        vQueueDelete(self->routing_event_queue);
        free(self->wireless);
        free(self->siblings);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Control plane initialized successfully");
    return ESP_OK;
}

void control_destroy(control_t *self) {
    if (!self) return;
    if (self->routing_event_queue) vQueueDelete(self->routing_event_queue);
    if (self->wireless) free(self->wireless);
    if (self->siblings) free(self->siblings);
    free(self);
}
