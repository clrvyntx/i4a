#include <string.h>
#include <inttypes.h>
#include "node.h"
#include "traffic.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "lwip/inet.h"
#include "info_manager/info_manager.h"
#include "esp_wifi.h"

#define SERVER_ADDRESS "example.com"
#define MAX_HTTP_OUTPUT_BUFFER 2048

#define CLIENT_POST_INTERVAL_MS (5 * 60 * 1000)     // 5 min
#define BROADCAST_INTERVAL_MS   (5 * 60 * 1000)     // 5 min
#define ORIENTATION_SPREAD_MS   (60 * 1000)         // 1 min per orientation

#define HTTP_CLIENT_TASK_CORE 1
#define HTTP_CLIENT_TASK_MEM 8092

#define IM_TASK_CORE 1
#define IM_TASK_MEM 4096

static const char *TAG = "info_manager";

static TaskHandle_t im_client_task_handle = NULL;
static TaskHandle_t im_scheduler_task_handle = NULL;

static im_manager_t info_manager = {0};
static im_manager_t *im = &info_manager;


static void im_client_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(CLIENT_POST_INTERVAL_MS));

    esp_http_client_config_t config = {
        .url = "http://" SERVER_ADDRESS,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
        // For HTTPS, add crt:
        // .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    char payload[MAX_HTTP_OUTPUT_BUFFER];

    while (true) {


        const im_ring_packet_t *ring = im_get_ring_info();
        int offset = 0;

        offset += snprintf(payload + offset, sizeof(payload) - offset, "[");

        for (int i = 0; i < MAX_ORIENTATIONS; i++) {

            if (i > 0)
                offset += snprintf(payload + offset, sizeof(payload) - offset, ",");

            char subnet_str[16];
            char mask_str[16];
            struct in_addr addr;

            addr.s_addr = htonl(ring[i].subnet);
            inet_ntoa_r(addr, subnet_str, sizeof(subnet_str));

            addr.s_addr = htonl(ring[i].mask);
            inet_ntoa_r(addr, mask_str, sizeof(mask_str));

            offset += snprintf(payload + offset, sizeof(payload) - offset,
                               "{\"orientation\":%d,"
                               "\"mac\":\"%s\","
                               "\"subnet\":\"%s\","
                               "\"mask\":\"%s\","
                               "\"is_root\":%u,"
                               "\"rssi\":%d,"
                               "\"rx_bytes\":%" PRIu64 ","
                               "\"tx_bytes\":%" PRIu64 "}",

                               ring[i].orientation,
                               ring[i].uuid,
                               subnet_str,
                               mask_str,
                               ring[i].is_root,
                               ring[i].rssi,
                               ring[i].rx_bytes,
                               ring[i].tx_bytes
            );

            if (offset >= MAX_HTTP_OUTPUT_BUFFER - 50) {
                ESP_LOGW(TAG, "Payload truncated");
                break;
            }
        }

        offset += snprintf(payload + offset, sizeof(payload) - offset, "]");

        ESP_LOGD(TAG, "Payload (%d): %s", offset, payload);


        esp_http_client_set_post_field(client, payload, strlen(payload));
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_method(client, HTTP_METHOD_POST);

        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            ESP_LOGI(TAG, "HTTP POST success, status=%d", status);
        } else {
            ESP_LOGW(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(CLIENT_POST_INTERVAL_MS));
    }

    esp_http_client_cleanup(client);
}

static void im_scheduler_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(node_get_device_orientation() * ORIENTATION_SPREAD_MS));

    while (true) {
        im_broadcast_info();
        vTaskDelay(pdMS_TO_TICKS(BROADCAST_INTERVAL_MS));
    }
}


static void im_on_sibling_message(void *ctx, const uint8_t *msg, uint16_t len) {
    if (len < sizeof(im_ring_packet_t)) return;

    const im_ring_packet_t *pkt = (const im_ring_packet_t *)msg;
    uint8_t idx = pkt->orientation;

    if (idx >= MAX_ORIENTATIONS) {
        ESP_LOGW(TAG, "Invalid orientation %d received", idx);
        return;
    }

    im->ring[idx] = *pkt;
    ESP_LOGD(TAG, "Updated info for orientation %d: UUID=%s", idx, pkt->uuid);
}

void im_init(ring_share_t *rs) {
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

bool im_broadcast_info(void) {
    if (!im->rs) {
        ESP_LOGW(TAG, "Cannot broadcast: ring_share not initialized");
        return false;
    }

    im_ring_packet_t pkt = {0};
    pkt.orientation = node_get_device_orientation();
    pkt.subnet = node_get_device_subnet();
    pkt.mask = node_get_device_mask();
    pkt.is_root = node_is_device_center_root();
    pkt.rssi = node_get_device_rssi();
    pkt.rx_bytes = node_traffic_get_rx_count();
    pkt.tx_bytes = node_traffic_get_tx_count();
    snprintf(pkt.uuid, sizeof(pkt.uuid), "%s", node_get_device_mac());
    snprintf(pkt.link, sizeof(pkt.link), "%s", node_get_link_name());

    im->ring[pkt.orientation] = pkt;

    return rs_broadcast(im->rs, RS_INFO_MANAGER, (uint8_t *)&pkt, sizeof(pkt));
}

const im_ring_packet_t *im_get_ring_info(void) {
    return im->ring;
}


void im_http_client_start(void) {
    if (im_client_task_handle != NULL) {
        ESP_LOGW(TAG, "HTTP client task already running");
        return;
    }

    xTaskCreatePinnedToCore(
        im_client_task,
        "im_client",
        HTTP_CLIENT_TASK_MEM,
        NULL,
        (tskIDLE_PRIORITY + 2),
        &im_client_task_handle,
        HTTP_CLIENT_TASK_CORE
    );

    ESP_LOGI(TAG, "Info manager HTTP client started");
}

void im_scheduler_start(void)
{
    if (im_scheduler_task_handle != NULL) {
        ESP_LOGW(TAG, "Scheduler task already running");
        return;
    }

    xTaskCreatePinnedToCore(
        im_scheduler_task,
        "im_scheduler",
        IM_TASK_MEM,
        NULL,
        (tskIDLE_PRIORITY + 2),
        &im_scheduler_task_handle,
        IM_TASK_CORE
    );

    ESP_LOGI(TAG, "Info manager scheduler started");
}
