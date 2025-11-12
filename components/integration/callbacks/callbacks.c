#include <string.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "callbacks.h"

#define MAX_MESSAGE_SIZE 512
#define SIBLING_QUEUE_LENGTH 10
#define PEER_QUEUE_LENGTH 5

static const char *TAG = "callbacks";

typedef struct message {
    uint8_t data[MAX_MESSAGE_SIZE];
    uint16_t length;
} message_t;

typedef enum {
    PEER_EVENT_CONNECTED,
    PEER_EVENT_LOST
} peer_event_type_t;

typedef struct {
    peer_event_type_t type;
    uint32_t net;
    uint32_t mask;
} peer_event_t;

static QueueHandle_t peer_event_queue = NULL;
static QueueHandle_t peer_message_queue = NULL;
static QueueHandle_t sibling_message_queue = NULL;

static wireless_t wireless = {0};
static siblings_t siblings = {0};

static wireless_t *wl = &wireless;
static siblings_t *sb = &siblings;

static void peer_event_task(void *arg) {
    peer_event_t event;
    while (1) {
        if (xQueueReceive(peer_event_queue, &event, portMAX_DELAY)) {
            if (event.type == PEER_EVENT_CONNECTED) {
                wl->callbacks.on_peer_connected(wl->context, event.net, event.mask);
            } else {
                wl->callbacks.on_peer_lost(wl->context, event.net, event.mask);
            }
        }
    }
}

static void peer_message_task(void *arg) {
    message_t m;
    while (1) {
        if (xQueueReceive(peer_message_queue, &m, portMAX_DELAY)) {
            wl->callbacks.on_peer_message(wl->context, m.data, m.length);
        }
    }
}

static void sibling_message_task(void *arg) {
    message_t m;
    while (1) {
        if (xQueueReceive(sibling_message_queue, &m, portMAX_DELAY)) {
            sb->callback(sb->context, m.data, m.length);
        }
    }
}

esp_err_t node_init_event_queues(void) {
    peer_event_queue = xQueueCreate(PEER_QUEUE_LENGTH, sizeof(peer_event_t));
    peer_message_queue = xQueueCreate(PEER_QUEUE_LENGTH, sizeof(message_t));
    sibling_message_queue = xQueueCreate(SIBLING_QUEUE_LENGTH, sizeof(message_t));

    if (!peer_event_queue || !peer_message_queue || !sibling_message_queue) {
        ESP_LOGE(TAG, "Failed to create one or more queues");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Event queues created successfully");
    return ESP_OK;
}

esp_err_t node_start_event_tasks(void) {
    BaseType_t res;

    res = xTaskCreatePinnedToCore(peer_event_task, "peer_event_task", 4096, NULL, (tskIDLE_PRIORITY + 2), NULL, 0);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create peer_event_task");
        return ESP_FAIL;
    }

    res = xTaskCreatePinnedToCore(peer_message_task, "peer_msg_task", 4096, NULL, (tskIDLE_PRIORITY + 2), NULL, 0);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create peer_message_task");
        return ESP_FAIL;
    }

    res = xTaskCreatePinnedToCore(sibling_message_task, "sib_msg_task", 4096, NULL, (tskIDLE_PRIORITY + 2), NULL, 0);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sibling_message_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "All event tasks created successfully");
    return ESP_OK;
}

void node_on_peer_connected(uint32_t net, uint32_t mask) {
    peer_event_t event = {
        .type = PEER_EVENT_CONNECTED,
        .net = net,
        .mask = mask
    };
    xQueueSend(peer_event_queue, &event, portMAX_DELAY);
}

void node_on_peer_lost(uint32_t net, uint32_t mask) {
    peer_event_t event = {
        .type = PEER_EVENT_LOST,
        .net = net,
        .mask = mask
    };
    xQueueSend(peer_event_queue, &event, portMAX_DELAY);
}

void node_on_peer_message(void *msg, uint16_t len) {
    message_t m;
    memcpy(m.data, msg, len);
    m.length = len;

    xQueueSend(peer_message_queue, &m, portMAX_DELAY);
}

void node_on_sibling_message(void *msg, uint16_t len) {
    message_t m;
    memcpy(m.data, msg, len);
    m.length = len;

    xQueueSend(sibling_message_queue, &m, portMAX_DELAY);
}

void node_register_wireless_callbacks(wireless_callbacks_t callbacks, void *context){
    wl->callbacks = callbacks;
    wl->context = context;
}

void node_register_siblings_callbacks( siblings_callback_t callback, void *context){
    sb->callback = callback;
    sb->context = context;
}

wireless_t *node_get_wireless_instance(void){
    return wl;
}

siblings_t *node_get_siblings_instance(void){
    return sb;
}



