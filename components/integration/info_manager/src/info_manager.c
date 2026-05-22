#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include "node.h"
#include "traffic.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "info_manager/info_manager.h"
#include "esp_wifi.h"
#include "task_config.h"

#define UDP_SERVER_IP   "10.255.255.254"
#define UDP_SERVER_PORT 8000

#define MAX_HTTP_OUTPUT_BUFFER 2048

#define CLIENT_POST_INTERVAL_MS (5 * 60 * 1000)     // 5 min
#define BROADCAST_INTERVAL_MS   (5 * 60 * 1000)     // 5 min
#define ORIENTATION_SPREAD_MS   (60 * 1000)         // 1 min per orientation

static const char *TAG = "info_manager";

static TaskHandle_t im_client_task_handle = NULL;
static TaskHandle_t im_scheduler_task_handle = NULL;

static im_manager_t info_manager = {0};
static im_manager_t *im = &info_manager;

static const char *orientation_names[MAX_ORIENTATIONS] = {
    "N", "S", "E", "W", "C"
};

static void im_client_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(CLIENT_POST_INTERVAL_MS));

    char payload[MAX_HTTP_OUTPUT_BUFFER];

    struct sockaddr_in dest_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(UDP_SERVER_PORT),
        .sin_addr.s_addr = inet_addr(UDP_SERVER_IP),
    };

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create UDP socket");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UDP socket created");

    while (true) {

        const im_ring_packet_t *ring = im_get_ring_info();

        int offset = 0;

        // Start JSON payload
        offset += snprintf(
            payload + offset,
            sizeof(payload) - offset,
            "{\"fields\":[\"orientation\",\"uuid\",\"link\","
            "\"subnet\",\"mask\",\"channel\",\"rssi\","
            "\"rx_bytes\",\"tx_bytes\"],\"data\":["
        );

        for (int i = 0; i < MAX_ORIENTATIONS; i++) {

            if (i > 0) {
                offset += snprintf(
                    payload + offset,
                    sizeof(payload) - offset,
                    ","
                );
            }

            char subnet_str[16];
            char mask_str[16];
            struct in_addr addr;

            addr.s_addr = htonl(ring[i].subnet);
            inet_ntoa_r(addr, subnet_str, sizeof(subnet_str));

            addr.s_addr = htonl(ring[i].mask);
            inet_ntoa_r(addr, mask_str, sizeof(mask_str));

            offset += snprintf(
                payload + offset,
                sizeof(payload) - offset,
                "[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%d,%d,%" PRIu64 ",%" PRIu64 "]",
                orientation_names[ring[i].orientation % MAX_ORIENTATIONS],
                ring[i].uuid,
                ring[i].link,
                subnet_str,
                mask_str,
                ring[i].channel,
                ring[i].rssi,
                ring[i].rx_bytes,
                ring[i].tx_bytes
            );

            if (offset >= MAX_HTTP_OUTPUT_BUFFER - 50) {
                ESP_LOGW(TAG, "Payload truncated");
                break;
            }
        }

        // Add heartbeat uptime
        offset += snprintf(
            payload + offset,
            sizeof(payload) - offset,
            "],\"uptime_mins\":%" PRId64 "}",
            node_get_device_uptime_minutes()
        );

        ESP_LOGD(TAG, "UDP Payload (%d): %s", offset, payload);

        int err = sendto(
            sock,
            payload,
            strlen(payload),
            0,
            (struct sockaddr *)&dest_addr,
            sizeof(dest_addr)
        );

        if (err < 0) {
            ESP_LOGW(TAG, "UDP send failed errno=%d", errno);
        } else {
            ESP_LOGI(TAG, "UDP packet sent (%d bytes)", err);
        }

        vTaskDelay(pdMS_TO_TICKS(CLIENT_POST_INTERVAL_MS));
    }

    close(sock);
}

static void im_scheduler_task(void *arg)
{
    vTaskDelay(
        pdMS_TO_TICKS(
            node_get_device_orientation() * ORIENTATION_SPREAD_MS
        )
    );

    while (true) {
        im_broadcast_info();
        vTaskDelay(pdMS_TO_TICKS(BROADCAST_INTERVAL_MS));
    }
}

static void im_on_sibling_message(
    void *ctx,
    const uint8_t *msg,
    uint16_t len
)
{
    if (len < sizeof(im_ring_packet_t))
        return;

    const im_ring_packet_t *pkt =
        (const im_ring_packet_t *)msg;

    uint8_t idx = pkt->orientation;

    if (idx >= MAX_ORIENTATIONS) {
        ESP_LOGW(TAG,
                 "Invalid orientation %d received",
                 idx);
        return;
    }

    im->ring[idx] = *pkt;

    ESP_LOGD(TAG,
             "Updated info for orientation %d: UUID=%s",
             idx,
             pkt->uuid);
}

void im_init(ring_share_t *rs)
{
    if (!rs) {
        ESP_LOGE(TAG, "Invalid parameters to im_init");
        return;
    }

    im->rs = rs;

    memset(im->ring, 0, sizeof(im->ring));

    rs_register_component(
        im->rs,
        RS_INFO_MANAGER,
        (ring_callback_t){
            .callback = im_on_sibling_message,
            .context = im
        }
    );

    ESP_LOGI(TAG, "Info manager initialized");
}

bool im_broadcast_info(void)
{
    if (!im->rs) {
        ESP_LOGW(TAG,
                 "Cannot broadcast: ring_share not initialized");
        return false;
    }

    im_ring_packet_t pkt = {0};

    pkt.orientation = node_get_device_orientation();
    pkt.channel     = node_get_device_channel();
    pkt.subnet      = node_get_device_subnet();
    pkt.mask        = node_get_device_mask();
    pkt.rssi        = node_get_device_rssi();

    pkt.rx_bytes = node_traffic_get_rx_count();
    pkt.tx_bytes = node_traffic_get_tx_count();

    snprintf(pkt.uuid,
             sizeof(pkt.uuid),
             "%s",
             node_get_device_mac());

    snprintf(pkt.link,
             sizeof(pkt.link),
             "%s",
             node_get_link_name());

    im->ring[pkt.orientation] = pkt;

    return rs_broadcast(
        im->rs,
        RS_INFO_MANAGER,
        (uint8_t *)&pkt,
        sizeof(pkt)
    );
}

const im_ring_packet_t *im_get_ring_info(void)
{
    return im->ring;
}

void im_http_client_start(void)
{
    if (im_client_task_handle != NULL) {
        ESP_LOGW(TAG,
                 "UDP client task already running");
        return;
    }

    xTaskCreatePinnedToCore(
        im_client_task,
        "im_client",
        TASK_HTTP_CLIENT_STACK,
        NULL,
        TASK_HTTP_CLIENT_PRIORITY,
        &im_client_task_handle,
        TASK_HTTP_CLIENT_CORE
    );

    ESP_LOGI(TAG, "Info manager UDP client started");
}

void im_scheduler_start(void)
{
    if (im_scheduler_task_handle != NULL) {
        ESP_LOGW(TAG,
                 "Scheduler task already running");
        return;
    }

    xTaskCreatePinnedToCore(
        im_scheduler_task,
        "im_scheduler",
        TASK_IM_SCHEDULER_STACK,
        NULL,
        TASK_IM_SCHEDULER_PRIORITY,
        &im_scheduler_task_handle,
        TASK_IM_SCHEDULER_CORE
    );

    ESP_LOGI(TAG, "Info manager scheduler started");
}
