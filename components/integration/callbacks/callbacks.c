#include <string.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "callbacks.h"

#define MAX_MESSAGE_SIZE 256
#define QUEUE_LENGTH 10

static const char *TAG = "callbacks";
static char uuid[7] = {0};

typedef struct message {
    uint8_t data[MAX_MESSAGE_SIZE];
    uint16_t length;
} message_t;

typedef struct peer_event {
    uint32_t net;
    uint32_t mask;
} peer_event_t;

static QueueHandle_t peer_connected_queue = NULL;
static QueueHandle_t peer_lost_queue = NULL;
static QueueHandle_t peer_message_queue = NULL;
static QueueHandle_t sibling_message_queue = NULL;

static void do_nothing_peer(void *ctx, uint32_t net, uint32_t mask) {
}

static void do_nothing_message(void *ctx, const uint8_t *data, uint16_t len) {
}

static wireless_t wireless = {
    .callbacks = {
        .on_peer_connected = do_nothing_peer,
        .on_peer_lost      = do_nothing_peer,
        .on_peer_message   = do_nothing_message,
    },
    .context = NULL,
};

static void read_uuid(void *ctx, const uint8_t *data, uint16_t len) {
    if (len >= sizeof(uuid)) {
        ESP_LOGW(TAG, "Received UUID too long, ignoring");
        return;
    }

    memcpy(uuid, data, len);
    uuid[len] = '\0';  // Null-terminate the string
}

static siblings_t siblings = {
    .callback = read_uuid,
    .context = NULL,
};

static wireless_t *wl = &wireless;
static siblings_t *sb = &siblings;

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

esp_err_t node_init_event_queues(void) {
    peer_connected_queue = xQueueCreate(QUEUE_LENGTH, sizeof(peer_event_t));
    peer_lost_queue      = xQueueCreate(QUEUE_LENGTH, sizeof(peer_event_t));
    peer_message_queue   = xQueueCreate(QUEUE_LENGTH, sizeof(message_t));
    sibling_message_queue= xQueueCreate(QUEUE_LENGTH, sizeof(message_t));

    if (!peer_connected_queue ||
        !peer_lost_queue ||
        !peer_message_queue ||
        !sibling_message_queue) {

        ESP_LOGE(TAG, "Failed to create one or more queues");
    return ESP_FAIL;
        }

        ESP_LOGI(TAG, "All event queues created successfully");
        return ESP_OK;
}

esp_err_t node_start_event_tasks(void) {
    BaseType_t res;

    res = xTaskCreate(peer_connected_task, "peer_conn_task", 4096, NULL, 5, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create peer_connected_task");
        return ESP_FAIL;
    }

    res = xTaskCreate(peer_lost_task, "peer_lost_task", 4096, NULL, 5, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create peer_lost_task");
        return ESP_FAIL;
    }

    res = xTaskCreate(peer_message_task, "peer_msg_task", 4096, NULL, 5, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create peer_message_task");
        return ESP_FAIL;
    }

    res = xTaskCreate(sibling_message_task, "sib_msg_task", 4096, NULL, 5, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sibling_message_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "All event tasks created successfully");
    return ESP_OK;
}

void node_on_peer_connected(uint32_t net, uint32_t mask) {
    peer_event_t event = {
        .net = net,
        .mask = mask,
    };

    xQueueSend(peer_connected_queue, &event, portMAX_DELAY);
}

void node_on_peer_lost(uint32_t net, uint32_t mask) {
    peer_event_t event = {
        .net = net,
        .mask = mask,
    };

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

wireless_t *node_get_wireless_instance(void){
    return wl;
}

siblings_t *node_get_siblings_instance(void){
    return sb;
}

const char *node_get_uuid(void) {
    return (const char *)uuid;
}
