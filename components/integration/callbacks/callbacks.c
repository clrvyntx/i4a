#include <string.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "node.h"
#include "callbacks.h"

#define MAX_MESSAGE_SIZE 512
#define QUEUE_LENGTH 10
#define SEND_DELAY_MS 500
#define RETRY_DELAY_MS 500
#define MAX_RETRIES 5

static const char *TAG = "callbacks";

/* Message and Event Structures */
typedef struct message {
    uint8_t data[MAX_MESSAGE_SIZE];
    uint16_t length;
} message_t;

typedef struct peer_event {
    uint32_t net;
    uint32_t mask;
} peer_event_t;

/* Event Queues */
static QueueHandle_t peer_connected_queue = NULL;
static QueueHandle_t peer_lost_queue      = NULL;
static QueueHandle_t peer_message_queue   = NULL;
static QueueHandle_t sibling_message_queue= NULL;
static QueueHandle_t peer_send_queue      = NULL;
static QueueHandle_t sibling_send_queue   = NULL;

/* Wireless and Sibling Instances */
static wireless_t wireless = {0};
static siblings_t siblings = {0};

static wireless_t *wl = &wireless;
static siblings_t *sb = &siblings;

/* --- Event Tasks --- */
static void peer_connected_task(void *arg) {
    peer_event_t event;
    while (1) {
        if (xQueueReceive(peer_connected_queue, &event, portMAX_DELAY)) {
            wl->callbacks.on_peer_connected(wl->context, event.net, event.mask);
        }
    }
}

static void peer_lost_task(void *arg) {
    peer_event_t event;
    while (1) {
        if (xQueueReceive(peer_lost_queue, &event, portMAX_DELAY)) {
            wl->callbacks.on_peer_lost(wl->context, event.net, event.mask);
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

/* --- Send Tasks (to serialize transmissions) --- */
static void peer_send_task(void *arg) {
    message_t m;
    while (1) {
        if (xQueueReceive(peer_send_queue, &m, portMAX_DELAY)) {
            int attempt = 0;
            bool sent = false;
            while (attempt < MAX_RETRIES && !sent) {
                sent = node_single_wireless_message(m.data, m.length);
                if (!sent) {
                    attempt++;
                    vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
                }
            }
            vTaskDelay(pdMS_TO_TICKS(SEND_DELAY_MS)); // spacing between messages
        }
    }
}

static void sibling_send_task(void *arg) {
    message_t m;
    while (1) {
        if (xQueueReceive(sibling_send_queue, &m, portMAX_DELAY)) {
            int attempt = 0;
            bool sent = false;
            while (attempt < MAX_RETRIES && !sent) {
                sent = node_single_sibling_broadcast(m.data, m.length);
                if (!sent) {
                    attempt++;
                    vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
                }
            }
            vTaskDelay(pdMS_TO_TICKS(SEND_DELAY_MS)); // spacing between messages
        }
    }
}

/* --- Initialization --- */
esp_err_t node_init_event_queues(void) {
    peer_connected_queue   = xQueueCreate(QUEUE_LENGTH, sizeof(peer_event_t));
    peer_lost_queue        = xQueueCreate(QUEUE_LENGTH, sizeof(peer_event_t));
    peer_message_queue     = xQueueCreate(QUEUE_LENGTH, sizeof(message_t));
    sibling_message_queue  = xQueueCreate(QUEUE_LENGTH, sizeof(message_t));
    peer_send_queue        = xQueueCreate(QUEUE_LENGTH, sizeof(message_t));
    sibling_send_queue     = xQueueCreate(QUEUE_LENGTH, sizeof(message_t));

    if (!peer_connected_queue || !peer_lost_queue || !peer_message_queue ||
        !sibling_message_queue || !peer_send_queue || !sibling_send_queue) {
        ESP_LOGE(TAG, "Failed to create one or more queues");
    return ESP_FAIL;
        }

        ESP_LOGI(TAG, "All event queues created successfully");
        return ESP_OK;
}

esp_err_t node_start_event_tasks(void) {
    BaseType_t res;

    res = xTaskCreatePinnedToCore(peer_connected_task, "peer_conn_task", 4096, NULL, tskIDLE_PRIORITY + 2, NULL, 1);
    if (res != pdPASS) { ESP_LOGE(TAG, "Failed to create peer_connected_task"); return ESP_FAIL; }

    res = xTaskCreatePinnedToCore(peer_lost_task, "peer_lost_task", 4096, NULL, tskIDLE_PRIORITY + 2, NULL, 1);
    if (res != pdPASS) { ESP_LOGE(TAG, "Failed to create peer_lost_task"); return ESP_FAIL; }

    res = xTaskCreatePinnedToCore(peer_message_task, "peer_msg_task", 4096, NULL, tskIDLE_PRIORITY + 2, NULL, 1);
    if (res != pdPASS) { ESP_LOGE(TAG, "Failed to create peer_message_task"); return ESP_FAIL; }

    res = xTaskCreatePinnedToCore(sibling_message_task, "sib_msg_task", 4096, NULL, tskIDLE_PRIORITY + 2, NULL, 1);
    if (res != pdPASS) { ESP_LOGE(TAG, "Failed to create sibling_message_task"); return ESP_FAIL; }

    res = xTaskCreatePinnedToCore(peer_send_task, "peer_send_task", 4096, NULL, tskIDLE_PRIORITY + 2, NULL, 1);
    if (res != pdPASS) { ESP_LOGE(TAG, "Failed to create peer_send_task"); return ESP_FAIL; }

    res = xTaskCreatePinnedToCore(sibling_send_task, "sib_send_task", 4096, NULL, tskIDLE_PRIORITY + 2, NULL, 1);
    if (res != pdPASS) { ESP_LOGE(TAG, "Failed to create sibling_send_task"); return ESP_FAIL; }

    ESP_LOGI(TAG, "All event tasks created successfully");
    return ESP_OK;
}

/* --- Event Enqueue Functions --- */
void node_on_peer_connected(uint32_t net, uint32_t mask) {
    peer_event_t event = { .net = net, .mask = mask };
    xQueueSend(peer_connected_queue, &event, portMAX_DELAY);
}

void node_on_peer_lost(uint32_t net, uint32_t mask) {
    peer_event_t event = { .net = net, .mask = mask };
    xQueueSend(peer_lost_queue, &event, portMAX_DELAY);
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

/* --- Send Queue Enqueue Functions --- */
bool node_enqueue_peer_send(void *msg, uint16_t len) {
    message_t m;
    memcpy(m.data, msg, len);
    m.length = len;
    return xQueueSend(peer_send_queue, &m, portMAX_DELAY) == pdTRUE;
}

bool node_enqueue_sibling_send(void *msg, uint16_t len) {
    message_t m;
    memcpy(m.data, msg, len);
    m.length = len;
    return xQueueSend(sibling_send_queue, &m, portMAX_DELAY) == pdTRUE;
}

/* --- Callback Registration --- */
void node_register_wireless_callbacks(wireless_callbacks_t callbacks, void *context) {
    wl->callbacks = callbacks;
    wl->context = context;
}

void node_register_siblings_callbacks(siblings_callback_t callback, void *context) {
    sb->callback = callback;
    sb->context = context;
}

/* --- Get Instances --- */
wireless_t *node_get_wireless_instance(void) {
    return wl;
}

siblings_t *node_get_siblings_instance(void) {
    return sb;
}
