#include "antenna_orientation_mode/antenna_orientation_mode.h"

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dhcpserver/dhcpserver.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "lwip/esp_netif_net_stack.h"
#include "config_portal/configuration_ap_filter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "node.h"
#include "reset_manager/reset_manager.h"

#define AOM_MODE_GPIO GPIO_NUM_33
#define AOM_MODE_ACTIVE_LEVEL 0
#define AOM_ENABLE_GPIO33_SELECTION 0
#define AOM_FORCE_ORIENTATION_MODE 0

#define AOM_AP_SSID "Orientacion_Antenas"
#define AOM_AP_PASSWORD "12345678"
#define AOM_AP_CHANNEL 1
#define AOM_AP_MAX_CONNECTIONS 4

#define AOM_PORTAL_IP_A 192
#define AOM_PORTAL_IP_B 168
#define AOM_PORTAL_IP_C 50
#define AOM_PORTAL_IP_D 1
#define AOM_PORTAL_IP_STRING "192.168.50.1"
#define AOM_PORTAL_NETMASK_A 255
#define AOM_PORTAL_NETMASK_B 255
#define AOM_PORTAL_NETMASK_C 255
#define AOM_PORTAL_NETMASK_D 0

#define AOM_HTTP_PORT 80
#define AOM_DNS_PORT 53
#define AOM_HTTP_CLOSE_DELAY_MS 50

#define AOM_SCAN_PREFIX "I4A"
#define AOM_SCAN_PERIOD_MS 1000
#define AOM_SERIAL_REFRESH_MS 5000
#define AOM_WEB_REFRESH_MS 5000
#define AOM_START_PERIOD_MS 2000
#define AOM_SCAN_STAGGER_MS 180
#define AOM_RESTART_DELAY_MS 1000
#define AOM_RESET_BROADCAST_WAIT_MS 2500
#define AOM_RESET_RETRY_COUNT 20

#define AOM_DIRECTIONAL_COUNT 4
#define AOM_MAX_REPORT_ENTRIES 8
#define AOM_SCAN_RESULT_CAPACITY 32
#define AOM_SSID_LENGTH 33
#define AOM_START_EVENT_BIT BIT0

#define AOM_SCANNER_TASK_STACK 8192
#define AOM_HTTP_TASK_STACK 6144
#define AOM_SERVICE_TASK_STACK 4096
#define AOM_TASK_PRIORITY 5

#define AOM_HTTP_REQUEST_LENGTH 768
#define AOM_HTTP_HEADER_LENGTH 256
#define AOM_HTTP_PATH_LENGTH 160
#define AOM_JSON_BUFFER_LENGTH 8192
#define AOM_DNS_PACKET_LENGTH 512
#define AOM_DNS_HEADER_LENGTH 12

#define AOM_STRINGIFY_INNER(value) #value
#define AOM_STRINGIFY(value) AOM_STRINGIFY_INNER(value)

static const char *TAG = "AOM";
static const char *HTTP_TAG = "AOM_SIMPLE_HTTP";

typedef enum {
    AOM_MSG_START = 1,
    AOM_MSG_REPORT = 2,
} aom_message_type_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
} aom_start_message_t;

typedef struct __attribute__((packed)) {
    char ssid[AOM_SSID_LENGTH];
    int8_t rssi;
    uint8_t channel;
} aom_report_entry_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t orientation;
    uint8_t entry_count;
    aom_report_entry_t entries[AOM_MAX_REPORT_ENTRIES];
} aom_report_message_t;

_Static_assert(sizeof(aom_report_message_t) <= RS_MAX_BROADCAST_LEN,
               "AOM report exceeds ring_share payload capacity");

typedef struct {
    bool received;
    int64_t received_at_us;
    uint8_t entry_count;
    aom_report_entry_t entries[AOM_MAX_REPORT_ENTRIES];
} aom_orientation_report_t;

typedef struct {
    ring_share_t *rs;
    bool is_central;
    SemaphoreHandle_t reports_mutex;
    EventGroupHandle_t start_event;
    aom_orientation_report_t reports[AOM_DIRECTIONAL_COUNT];
} aom_context_t;

static const char *const AOM_ORIENTATION_NAMES[AOM_DIRECTIONAL_COUNT] = {
    "NORTE",
    "SUR",
    "ESTE",
    "OESTE",
};

static aom_context_t *s_aom = NULL;
static esp_netif_t *s_ap_netif = NULL;
static volatile bool s_restart_pending = false;

static const char AOM_PORTAL_HTML[] =
    "<!doctype html><html lang=\"es\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Orientacion de antenas</title><style>"
    ":root{font-family:system-ui,-apple-system,sans-serif;color:#17212b}"
    "body{margin:0;background:#eef3f8;min-height:100vh;padding:24px 0}"
    "main{box-sizing:border-box;width:min(94vw,900px);margin:auto;background:#fff;"
    "padding:24px;border-radius:16px;box-shadow:0 12px 36px #17324d24}"
    "h1{margin:0 0 8px;color:#075985}p{color:#475569}"
    "table{width:100%;border-collapse:collapse;margin-top:22px}"
    "th,td{text-align:left;padding:11px;border-bottom:1px solid #cbd5e1}"
    "th{color:#075985}#status{font-size:.9rem;color:#64748b}"
    "button{width:100%;margin-top:24px;padding:13px;border:0;border-radius:9px;"
    "background:#92400e;color:#fff;font-weight:700;font-size:1rem}"
    "</style></head><body><main><h1>Orientacion de antenas</h1>"
    "<p>Redes detectadas cuyo SSID comienza con <strong>I4A</strong>.</p>"
    "<div id=\"status\">Esperando reportes...</div>"
    "<table><thead><tr><th>Antena</th><th>SSID</th><th>RSSI</th>"
    "<th>Canal</th><th>Hace</th></tr></thead><tbody id=\"rows\"></tbody></table>"
    "<form method=\"post\" action=\"/exit\" "
    "onsubmit=\"return confirm('Finalizar la orientacion y reiniciar los cinco ESP32?')\">"
    "<button type=\"submit\">Finalizar orientacion y reiniciar normalmente</button></form>"
    "<script>"
    "function cell(v){var d=document.createElement('td');d.textContent=v;return d;}"
    "function addRow(o,s,r,c,a){var tr=document.createElement('tr');"
    "tr.appendChild(cell(o));tr.appendChild(cell(s));tr.appendChild(cell(r));"
    "tr.appendChild(cell(c));tr.appendChild(cell(a));"
    "document.getElementById('rows').appendChild(tr);}"
    "function render(data){var rows=document.getElementById('rows');rows.innerHTML='';"
    "for(var i=0;i<data.reports.length;i++){var p=data.reports[i];"
    "var age=p.age_s<0?'--':p.age_s+' s';"
    "if(!p.entries.length){addRow(p.orientation,'--','--','--',age);continue;}"
    "for(var j=0;j<p.entries.length;j++){var e=p.entries[j];"
    "addRow(p.orientation,e.ssid,e.rssi+' dBm',e.channel,age);}}"
    "document.getElementById('status').textContent='Actualizado';}"
    "function refresh(){var x=new XMLHttpRequest();x.onreadystatechange=function(){"
    "if(x.readyState===4){if(x.status===200){try{render(JSON.parse(x.responseText));}"
    "catch(e){document.getElementById('status').textContent='Respuesta invalida';}}"
    "else{document.getElementById('status').textContent='Sin respuesta del ESP';}}};"
    "x.open('GET','/api/reports',true);x.send();}"
    "refresh();setInterval(refresh," AOM_STRINGIFY(AOM_WEB_REFRESH_MS) ");"
    "</script></main></body></html>";

static const char AOM_EXIT_HTML[] =
    "<!doctype html><html lang=\"es\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Orientacion de antenas</title></head><body style=\"font-family:system-ui;margin:2rem\">"
    "<h1>Reinicio programado</h1>"
    "<p>El nodo saldra del modo orientacion y arrancara normalmente.</p>"
    "</body></html>";

static bool aom_is_central(void) {
    return node_get_device_orientation() == NODE_DEVICE_ORIENTATION_CENTER;
}

bool antenna_orientation_mode_pin_is_active(void) {
#if AOM_FORCE_ORIENTATION_MODE
    ESP_LOGW(TAG, "modo orientacion forzado por software");
    return true;
#elif AOM_ENABLE_GPIO33_SELECTION
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << AOM_MODE_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no se pudo configurar GPIO%d: %s", AOM_MODE_GPIO,
                 esp_err_to_name(err));
        return false;
    }

    int level = gpio_get_level(AOM_MODE_GPIO);
    bool active = level == AOM_MODE_ACTIVE_LEVEL;
    ESP_LOGI(TAG, "pin modo orientacion GPIO=%d level=%d active=%d",
             AOM_MODE_GPIO, level, active);
    return active;
#else
    ESP_LOGI(TAG, "seleccion de orientacion por GPIO33 desactivada");
    return false;
#endif
}

static size_t aom_report_message_length(uint8_t entry_count) {
    return offsetof(aom_report_message_t, entries) +
           (size_t)entry_count * sizeof(aom_report_entry_t);
}

static bool aom_copy_reports(
    aom_orientation_report_t reports[AOM_DIRECTIONAL_COUNT]) {
    if (s_aom == NULL || s_aom->reports_mutex == NULL ||
        xSemaphoreTake(s_aom->reports_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    memcpy(reports, s_aom->reports, sizeof(s_aom->reports));
    xSemaphoreGive(s_aom->reports_mutex);
    return true;
}

static void aom_on_sibling_message(void *context, const uint8_t *msg,
                                   uint16_t len) {
    aom_context_t *aom = (aom_context_t *)context;
    if (aom == NULL || msg == NULL || len < 1) {
        return;
    }

    if (msg[0] == AOM_MSG_START) {
        if (!aom->is_central && len == sizeof(aom_start_message_t) &&
            aom->start_event != NULL) {
            xEventGroupSetBits(aom->start_event, AOM_START_EVENT_BIT);
        }
        return;
    }

    if (msg[0] != AOM_MSG_REPORT || !aom->is_central ||
        len < offsetof(aom_report_message_t, entries)) {
        return;
    }

    aom_report_message_t packet = {0};
    size_t copy_length = len < sizeof(packet) ? len : sizeof(packet);
    memcpy(&packet, msg, copy_length);
    if (packet.orientation >= AOM_DIRECTIONAL_COUNT ||
        packet.entry_count > AOM_MAX_REPORT_ENTRIES ||
        len != aom_report_message_length(packet.entry_count)) {
        ESP_LOGW(TAG, "reporte invalido len=%u orientation=%u entries=%u",
                 len, packet.orientation, packet.entry_count);
        return;
    }

    if (xSemaphoreTake(aom->reports_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "no se pudo guardar el reporte RSSI");
        return;
    }
    aom_orientation_report_t *destination =
        &aom->reports[packet.orientation];
    destination->received = true;
    destination->received_at_us = esp_timer_get_time();
    destination->entry_count = packet.entry_count;
    memcpy(destination->entries, packet.entries,
           packet.entry_count * sizeof(packet.entries[0]));
    xSemaphoreGive(aom->reports_mutex);
}

static void aom_start_broadcast_task(void *argument) {
    aom_context_t *aom = (aom_context_t *)argument;
    const aom_start_message_t start = {
        .type = AOM_MSG_START,
    };

    while (true) {
        if (!rs_broadcast(aom->rs, RS_ANTENNA_ORIENTATION_MODE,
                          &start, sizeof(start))) {
            ESP_LOGW(TAG, "no se pudo difundir AOM_MSG_START");
        }
        vTaskDelay(pdMS_TO_TICKS(AOM_START_PERIOD_MS));
    }
}

static esp_err_t aom_start_scanner_wifi(void) {
    if (esp_netif_create_default_wifi_sta() == NULL) {
        return ESP_FAIL;
    }

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) {
        err = esp_wifi_start();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no se pudo iniciar WiFi STA para escaneo: %s",
                 esp_err_to_name(err));
    }
    return err;
}

static void aom_scan_task(void *argument) {
    aom_context_t *aom = (aom_context_t *)argument;
    xEventGroupWaitBits(aom->start_event, AOM_START_EVENT_BIT, pdFALSE,
                        pdTRUE, portMAX_DELAY);

    uint32_t stagger_ms =
        (uint32_t)node_get_device_orientation() * AOM_SCAN_STAGGER_MS;
    vTaskDelay(pdMS_TO_TICKS(stagger_ms));

    if (aom_start_scanner_wifi() != ESP_OK) {
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "START recibido; escaneando prefijo %s cada %d ms",
             AOM_SCAN_PREFIX, AOM_SCAN_PERIOD_MS);
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 30,
                .max = 60,
            },
        },
    };

    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        aom_report_message_t report = {
            .type = AOM_MSG_REPORT,
            .orientation = (uint8_t)node_get_device_orientation(),
            .entry_count = 0,
        };

        esp_err_t err = esp_wifi_scan_start(&scan_config, true);
        if (err == ESP_OK) {
            uint16_t found_count = 0;
            err = esp_wifi_scan_get_ap_num(&found_count);
            if (err == ESP_OK && found_count > 0) {
                wifi_ap_record_t records[AOM_SCAN_RESULT_CAPACITY];
                uint16_t records_count = found_count;
                if (records_count > AOM_SCAN_RESULT_CAPACITY) {
                    records_count = AOM_SCAN_RESULT_CAPACITY;
                }
                err = esp_wifi_scan_get_ap_records(&records_count, records);
                if (err == ESP_OK) {
                    for (uint16_t index = 0;
                         index < records_count &&
                         report.entry_count < AOM_MAX_REPORT_ENTRIES;
                         index++) {
                        if (strncmp((const char *)records[index].ssid,
                                    AOM_SCAN_PREFIX,
                                    strlen(AOM_SCAN_PREFIX)) != 0) {
                            continue;
                        }

                        aom_report_entry_t *entry =
                            &report.entries[report.entry_count++];
                        memcpy(entry->ssid, records[index].ssid,
                               AOM_SSID_LENGTH - 1);
                        entry->ssid[AOM_SSID_LENGTH - 1] = '\0';
                        entry->rssi = records[index].rssi;
                        entry->channel = records[index].primary;
                    }
                }
            }
        }

        if (err != ESP_OK) {
            ESP_LOGW(TAG, "escaneo WiFi fallido: %s", esp_err_to_name(err));
        }

        size_t report_length =
            aom_report_message_length(report.entry_count);
        if (!rs_broadcast(aom->rs, RS_ANTENNA_ORIENTATION_MODE,
                          &report, report_length)) {
            ESP_LOGW(TAG, "no se pudo enviar AOM_MSG_REPORT");
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(AOM_SCAN_PERIOD_MS));
    }
}

static void aom_serial_task(void *argument) {
    (void)argument;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(AOM_SERIAL_REFRESH_MS));
        aom_orientation_report_t reports[AOM_DIRECTIONAL_COUNT];
        if (!aom_copy_reports(reports)) {
            continue;
        }

        int64_t now_us = esp_timer_get_time();
        ESP_LOGI(TAG, "Antena | SSID | RSSI | Canal | Hace");
        for (size_t orientation = 0; orientation < AOM_DIRECTIONAL_COUNT;
             orientation++) {
            if (!reports[orientation].received) {
                ESP_LOGI(TAG, "%s | -- | -- | -- | sin reporte",
                         AOM_ORIENTATION_NAMES[orientation]);
                continue;
            }

            int64_t age_s =
                (now_us - reports[orientation].received_at_us) / 1000000;
            if (reports[orientation].entry_count == 0) {
                ESP_LOGI(TAG, "%s | -- | -- | -- | %lld s",
                         AOM_ORIENTATION_NAMES[orientation],
                         (long long)age_s);
                continue;
            }

            for (size_t entry = 0;
                 entry < reports[orientation].entry_count; entry++) {
                ESP_LOGI(TAG, "%s | %s | %d dBm | %u | %lld s",
                         AOM_ORIENTATION_NAMES[orientation],
                         reports[orientation].entries[entry].ssid,
                         reports[orientation].entries[entry].rssi,
                         reports[orientation].entries[entry].channel,
                         (long long)age_s);
            }
        }
    }
}

static bool aom_send_all(int socket_fd, const void *data, size_t length) {
    const uint8_t *cursor = (const uint8_t *)data;
    while (length > 0) {
        int sent = send(socket_fd, cursor, length, 0);
        if (sent <= 0) {
            return false;
        }
        cursor += sent;
        length -= (size_t)sent;
    }
    return true;
}

static bool aom_send_http_response(int client_sock, const char *status,
                                   const char *content_type,
                                   const char *body, size_t body_length) {
    char header[AOM_HTTP_HEADER_LENGTH];
    int header_length = snprintf(
        header, sizeof(header),
        "HTTP/1.1 %s\x0D\x0A"
        "Content-Type: %s\x0D\x0A"
        "Cache-Control: no-store\x0D\x0A"
        "Content-Length: %u\x0D\x0A"
        "Connection: close\x0D\x0A"
        "\x0D\x0A",
        status, content_type, (unsigned int)body_length);
    if (header_length < 0 || header_length >= (int)sizeof(header)) {
        return false;
    }

    return aom_send_all(client_sock, header, (size_t)header_length) &&
           (body_length == 0 ||
            aom_send_all(client_sock, body, body_length));
}

static bool aom_send_http_redirect(int client_sock) {
    static const char response[] =
        "HTTP/1.1 302 Found\x0D\x0A"
        "Location: /\x0D\x0A"
        "Cache-Control: no-store\x0D\x0A"
        "Content-Length: 0\x0D\x0A"
        "Connection: close\x0D\x0A"
        "\x0D\x0A";
    return aom_send_all(client_sock, response, sizeof(response) - 1);
}

static bool aom_json_append(char *buffer, size_t capacity, size_t *offset,
                            const char *format, ...) {
    if (*offset >= capacity) {
        return false;
    }

    va_list args;
    va_start(args, format);
    int written = vsnprintf(buffer + *offset, capacity - *offset,
                            format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= capacity - *offset) {
        return false;
    }
    *offset += (size_t)written;
    return true;
}

static bool aom_json_append_ssid(char *buffer, size_t capacity,
                                 size_t *offset, const char *ssid) {
    for (size_t index = 0; index < AOM_SSID_LENGTH && ssid[index] != '\0';
         index++) {
        uint8_t value = (uint8_t)ssid[index];
        if (value == '"' || value == '\\') {
            if (!aom_json_append(buffer, capacity, offset, "\\%c", value)) {
                return false;
            }
        } else if (value < 0x20 || value >= 0x7F) {
            if (!aom_json_append(buffer, capacity, offset, "\\u%04x", value)) {
                return false;
            }
        } else if (!aom_json_append(buffer, capacity, offset, "%c", value)) {
            return false;
        }
    }
    return true;
}

static char *aom_create_reports_json(size_t *json_length) {
    aom_orientation_report_t reports[AOM_DIRECTIONAL_COUNT];
    if (!aom_copy_reports(reports)) {
        return NULL;
    }

    char *json = malloc(AOM_JSON_BUFFER_LENGTH);
    if (json == NULL) {
        return NULL;
    }

    size_t offset = 0;
    bool ok = aom_json_append(json, AOM_JSON_BUFFER_LENGTH, &offset,
                              "{\"prefix\":\"%s\",\"reports\":[",
                              AOM_SCAN_PREFIX);
    int64_t now_us = esp_timer_get_time();
    for (size_t orientation = 0;
         ok && orientation < AOM_DIRECTIONAL_COUNT; orientation++) {
        int64_t age_s = reports[orientation].received
                            ? (now_us - reports[orientation].received_at_us) /
                                  1000000
                            : -1;
        ok = aom_json_append(
            json, AOM_JSON_BUFFER_LENGTH, &offset,
            "%s{\"orientation\":\"%s\",\"age_s\":%lld,\"entries\":[",
            orientation == 0 ? "" : ",",
            AOM_ORIENTATION_NAMES[orientation], (long long)age_s);

        for (size_t entry = 0;
             ok && entry < reports[orientation].entry_count; entry++) {
            ok = aom_json_append(json, AOM_JSON_BUFFER_LENGTH, &offset,
                                 "%s{\"ssid\":\"",
                                 entry == 0 ? "" : ",") &&
                 aom_json_append_ssid(
                     json, AOM_JSON_BUFFER_LENGTH, &offset,
                     reports[orientation].entries[entry].ssid) &&
                 aom_json_append(
                     json, AOM_JSON_BUFFER_LENGTH, &offset,
                     "\",\"rssi\":%d,\"channel\":%u}",
                     reports[orientation].entries[entry].rssi,
                     reports[orientation].entries[entry].channel);
        }
        ok = ok && aom_json_append(json, AOM_JSON_BUFFER_LENGTH,
                                   &offset, "]}");
    }
    ok = ok && aom_json_append(json, AOM_JSON_BUFFER_LENGTH, &offset, "]}");
    if (!ok) {
        free(json);
        return NULL;
    }

    *json_length = offset;
    return json;
}

static bool aom_path_serves_portal(const char *path) {
    static const char *const portal_paths[] = {
        "/",
        "/generate_204",
        "/gen_204",
        "/hotspot-detect.html",
        "/ncsi.txt",
        "/connecttest.txt",
    };
    for (size_t index = 0;
         index < sizeof(portal_paths) / sizeof(portal_paths[0]); index++) {
        if (strcmp(path, portal_paths[index]) == 0) {
            return true;
        }
    }
    return false;
}

static void aom_restart_normal_task(void *argument) {
    (void)argument;
    vTaskDelay(pdMS_TO_TICKS(AOM_RESTART_DELAY_MS));

    bool reset_broadcasted = false;
    for (size_t attempt = 0;
         attempt < AOM_RESET_RETRY_COUNT && !reset_broadcasted; attempt++) {
        reset_broadcasted = rm_broadcast_reset();
        if (!reset_broadcasted) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    if (!reset_broadcasted) {
        ESP_LOGW(TAG,
                 "reinicio no recorrio todo el anillo; el central reintentara al arrancar");
    }

    vTaskDelay(pdMS_TO_TICKS(AOM_RESET_BROADCAST_WAIT_MS));
    esp_restart();
}

static esp_err_t aom_schedule_normal_restart(void) {
    if (s_restart_pending) {
        return ESP_ERR_INVALID_STATE;
    }

    s_restart_pending = true;
    if (xTaskCreate(aom_restart_normal_task, "aom_restart",
                    AOM_SERVICE_TASK_STACK, NULL, AOM_TASK_PRIORITY,
                    NULL) != pdPASS) {
        s_restart_pending = false;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static void aom_handle_http_client(int client_sock) {
    char request[AOM_HTTP_REQUEST_LENGTH];
    int received = recv(client_sock, request, sizeof(request) - 1, 0);
    if (received <= 0) {
        return;
    }
    request[received] = '\0';

    char method[8] = {0};
    char path[AOM_HTTP_PATH_LENGTH] = {0};
    if (sscanf(request, "%7s %159s", method, path) != 2) {
        aom_send_http_response(client_sock, "400 Bad Request", "text/plain",
                               "Bad Request", 11);
        return;
    }

    char *query = strchr(path, '?');
    if (query != NULL) {
        *query = '\0';
    }
    if (strcmp(method, "POST") == 0 && strcmp(path, "/exit") == 0) {
        esp_err_t err = aom_schedule_normal_restart();
        if (err == ESP_OK) {
            aom_send_http_response(client_sock, "202 Accepted",
                                   "text/html; charset=utf-8",
                                   AOM_EXIT_HTML,
                                   sizeof(AOM_EXIT_HTML) - 1);
        } else if (err == ESP_ERR_INVALID_STATE) {
            aom_send_http_response(client_sock, "409 Conflict", "text/plain",
                                   "Restart already scheduled", 25);
        } else {
            aom_send_http_response(client_sock,
                                   "500 Internal Server Error", "text/plain",
                                   "Unable to schedule restart", 26);
        }
    } else if (strcmp(method, "GET") != 0) {
        aom_send_http_response(client_sock, "405 Method Not Allowed",
                               "text/plain", "Method Not Allowed", 18);
    } else if (strcmp(path, "/api/reports") == 0) {
        size_t json_length = 0;
        char *json = aom_create_reports_json(&json_length);
        if (json == NULL) {
            aom_send_http_response(client_sock,
                                   "503 Service Unavailable", "text/plain",
                                   "Reports unavailable", 19);
        } else {
            aom_send_http_response(client_sock, "200 OK", "application/json",
                                   json, json_length);
            free(json);
        }
    } else if (aom_path_serves_portal(path)) {
        aom_send_http_response(client_sock, "200 OK",
                               "text/html; charset=utf-8", AOM_PORTAL_HTML,
                               sizeof(AOM_PORTAL_HTML) - 1);
    } else if (strcmp(path, "/favicon.ico") == 0) {
        aom_send_http_response(client_sock, "404 Not Found", "text/plain",
                               "", 0);
    } else {
        aom_send_http_redirect(client_sock);
    }
}

static void aom_simple_http_task(void *argument) {
    (void)argument;
    ESP_LOGI(HTTP_TAG, "tarea iniciada");

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock < 0) {
        ESP_LOGE(HTTP_TAG, "socket() fallo: errno=%d", errno);
        vTaskDelete(NULL);
        return;
    }

    int reuse = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(AOM_HTTP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(listen_sock, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        listen(listen_sock, AOM_AP_MAX_CONNECTIONS) < 0) {
        ESP_LOGE(HTTP_TAG, "bind/listen fallo: errno=%d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(HTTP_TAG, "escuchando en http://%s:%d/",
             AOM_PORTAL_IP_STRING, AOM_HTTP_PORT);
    while (true) {
        struct sockaddr_storage source;
        socklen_t source_length = sizeof(source);
        int client_sock = accept(listen_sock, (struct sockaddr *)&source,
                                 &source_length);
        if (client_sock < 0) {
            ESP_LOGW(HTTP_TAG, "accept() fallo: errno=%d", errno);
            continue;
        }

        struct timeval timeout = {
            .tv_sec = 3,
            .tv_usec = 0,
        };
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO,
                   &timeout, sizeof(timeout));
        aom_handle_http_client(client_sock);
        vTaskDelay(pdMS_TO_TICKS(AOM_HTTP_CLOSE_DELAY_MS));
        shutdown(client_sock, SHUT_RDWR);
        close(client_sock);
    }
}

static size_t aom_dns_question_end(const uint8_t *packet, size_t length) {
    size_t offset = AOM_DNS_HEADER_LENGTH;
    while (offset < length) {
        uint8_t label_length = packet[offset++];
        if (label_length == 0) {
            return offset + 4 <= length ? offset + 4 : 0;
        }
        if ((label_length & 0xC0) != 0 || offset + label_length > length) {
            return 0;
        }
        offset += label_length;
    }
    return 0;
}

static void aom_dns_task(void *argument) {
    (void)argument;
    int socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd < 0) {
        ESP_LOGE(TAG, "DNS socket() fallo: errno=%d", errno);
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(AOM_DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        ESP_LOGE(TAG, "DNS bind() fallo: errno=%d", errno);
        close(socket_fd);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "DNS captive portal iniciado en puerto %d", AOM_DNS_PORT);
    uint8_t packet[AOM_DNS_PACKET_LENGTH];
    while (true) {
        struct sockaddr_storage source;
        socklen_t source_length = sizeof(source);
        int received = recvfrom(socket_fd, packet, sizeof(packet), 0,
                                (struct sockaddr *)&source, &source_length);
        if (received < AOM_DNS_HEADER_LENGTH || packet[4] != 0 ||
            packet[5] != 1) {
            continue;
        }

        size_t response_length =
            aom_dns_question_end(packet, (size_t)received);
        if (response_length == 0 || response_length + 16 > sizeof(packet)) {
            continue;
        }

        packet[2] = 0x81;
        packet[3] = 0x80;
        packet[6] = 0;
        packet[7] = 1;
        packet[8] = 0;
        packet[9] = 0;
        packet[10] = 0;
        packet[11] = 0;
        static const uint8_t answer[] = {
            0xC0, 0x0C,
            0x00, 0x01,
            0x00, 0x01,
            0x00, 0x00, 0x00, 0x1E,
            0x00, 0x04,
            AOM_PORTAL_IP_A, AOM_PORTAL_IP_B,
            AOM_PORTAL_IP_C, AOM_PORTAL_IP_D,
        };
        memcpy(packet + response_length, answer, sizeof(answer));
        response_length += sizeof(answer);
        sendto(socket_fd, packet, response_length, 0,
               (struct sockaddr *)&source, source_length);
    }
}

static esp_err_t aom_start_access_point(void) {
    ESP_LOGI(TAG, "iniciando AP de portal cautivo");
    static const esp_netif_netstack_config_t portal_stack = {
        .lwip = {
            .init_fn = configuration_ap_filter_init,
            .input_fn = wlanif_input,
        },
    };
    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_WIFI_AP();
    netif_config.stack = &portal_stack;
    s_ap_netif = esp_netif_new(&netif_config);
    if (s_ap_netif == NULL) {
        return ESP_FAIL;
    }

    esp_err_t err = esp_netif_attach_wifi_ap(s_ap_netif);
    if (err == ESP_OK) {
        err = esp_wifi_set_default_wifi_ap_handlers();
    }
    if (err != ESP_OK) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
        return err;
    }

    err = esp_netif_dhcps_stop(s_ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        return err;
    }

    esp_netif_ip_info_t ip_info = {
        .ip = {.addr = ESP_IP4TOADDR(
                   AOM_PORTAL_IP_A, AOM_PORTAL_IP_B,
                   AOM_PORTAL_IP_C, AOM_PORTAL_IP_D)},
        .gw = {.addr = ESP_IP4TOADDR(
                   AOM_PORTAL_IP_A, AOM_PORTAL_IP_B,
                   AOM_PORTAL_IP_C, AOM_PORTAL_IP_D)},
        .netmask = {.addr = ESP_IP4TOADDR(
                   AOM_PORTAL_NETMASK_A, AOM_PORTAL_NETMASK_B,
                   AOM_PORTAL_NETMASK_C, AOM_PORTAL_NETMASK_D)},
    };
    err = esp_netif_set_ip_info(s_ap_netif, &ip_info);
    if (err != ESP_OK) {
        return err;
    }

    esp_netif_dns_info_t dns_info = {
        .ip = {
            .u_addr.ip4.addr = ESP_IP4TOADDR(
                AOM_PORTAL_IP_A, AOM_PORTAL_IP_B,
                AOM_PORTAL_IP_C, AOM_PORTAL_IP_D),
            .type = IPADDR_TYPE_V4,
        },
    };
    dhcps_offer_t dns_offer = OFFER_DNS;
    err = esp_netif_dhcps_option(
        s_ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
        &dns_offer, sizeof(dns_offer));
    if (err == ESP_OK) {
        err = esp_netif_set_dns_info(s_ap_netif, ESP_NETIF_DNS_MAIN,
                                     &dns_info);
    }
    if (err == ESP_OK) {
        err = esp_netif_dhcps_start(s_ap_netif);
    }
    if (err != ESP_OK) {
        return err;
    }

    wifi_config_t wifi_config = {0};
    memcpy(wifi_config.ap.ssid, AOM_AP_SSID, sizeof(AOM_AP_SSID));
    memcpy(wifi_config.ap.password, AOM_AP_PASSWORD,
           sizeof(AOM_AP_PASSWORD));
    wifi_config.ap.ssid_len = strlen(AOM_AP_SSID);
    wifi_config.ap.channel = AOM_AP_CHANNEL;
    wifi_config.ap.max_connection = AOM_AP_MAX_CONNECTIONS;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.ap.pmf_cfg.required = false;

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err == ESP_OK) {
        err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    }
    if (err == ESP_OK) {
        err = esp_wifi_start();
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "AP iniciado SSID=%s IP=%s", AOM_AP_SSID,
                 AOM_PORTAL_IP_STRING);
    }
    return err;
}

static esp_err_t aom_start_captive_portal(void) {
    ESP_LOGI(TAG, "aom_start_captive_portal inicio");
    esp_err_t err = aom_start_access_point();
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "creando servidor HTTP simple");
    if (xTaskCreate(aom_simple_http_task, "aom_http",
                    AOM_HTTP_TASK_STACK, NULL, AOM_TASK_PRIORITY,
                    NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(aom_dns_task, "aom_dns", AOM_SERVICE_TASK_STACK,
                    NULL, AOM_TASK_PRIORITY, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void antenna_orientation_mode_run(ring_share_t *rs) {
    if (rs == NULL) {
        ESP_LOGE(TAG, "ring_share invalido");
        return;
    }
    if (s_aom != NULL) {
        ESP_LOGW(TAG, "modo orientacion ya iniciado");
        return;
    }

    s_aom = calloc(1, sizeof(*s_aom));
    if (s_aom == NULL) {
        ESP_LOGE(TAG, "sin memoria para contexto AOM");
        return;
    }
    s_aom->rs = rs;
    s_aom->is_central = aom_is_central();
    s_aom->reports_mutex = xSemaphoreCreateMutex();
    if (s_aom->reports_mutex == NULL) {
        ESP_LOGE(TAG, "sin memoria para mutex de reportes");
        return;
    }
    if (!s_aom->is_central) {
        s_aom->start_event = xEventGroupCreate();
        if (s_aom->start_event == NULL) {
            ESP_LOGE(TAG, "sin memoria para evento START");
            return;
        }
    }

    rs_register_component(
        s_aom->rs, RS_ANTENNA_ORIENTATION_MODE,
        (ring_callback_t){
            .callback = aom_on_sibling_message,
            .context = s_aom,
        });
    ESP_LOGI(TAG, "callback registrado en RS_ANTENNA_ORIENTATION_MODE=%d",
             RS_ANTENNA_ORIENTATION_MODE);

    if (s_aom->is_central) {
        ESP_LOGI(TAG, "ESP central en modo orientacion");
        esp_err_t err = aom_start_captive_portal();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "portal cautivo no iniciado: %s",
                     esp_err_to_name(err));
            return;
        }
        if (xTaskCreate(aom_start_broadcast_task, "aom_start",
                        AOM_SERVICE_TASK_STACK, s_aom, AOM_TASK_PRIORITY,
                        NULL) != pdPASS ||
            xTaskCreate(aom_serial_task, "aom_serial",
                        AOM_SERVICE_TASK_STACK, s_aom, AOM_TASK_PRIORITY,
                        NULL) != pdPASS) {
            ESP_LOGE(TAG, "no se pudieron crear tareas centrales AOM");
        }
    } else {
        if (xTaskCreate(aom_scan_task, "aom_scan", AOM_SCANNER_TASK_STACK,
                        s_aom, AOM_TASK_PRIORITY, NULL) != pdPASS) {
            ESP_LOGE(TAG, "no se pudo crear tarea de escaneo AOM");
        }
    }
}
