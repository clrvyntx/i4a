#include "i4a_hal.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "virtual_nic.h"

#include "uart.h"

const char *TAG = "i4a_hal";

typedef struct {
    size_t len;
    uint8_t data[1600];
} spi_packet_t;

static struct {
    bool initialized;

    // Static storage
    StaticSemaphore_t _st_read_lock, _st_write_lock, _st_spi_queue;
    SemaphoreHandle_t read_lock, write_lock;

    QueueHandle_t spi_queue;

    struct {
        vnic_t ap_tx, ap_rx;
        vnic_t sta_tx, sta_rx;
        esp_netif_t *ap_netif, *sta_netif;
        wifi_mode_t mode;
    } wlan;
} hal = { 0 };

static void uart_write_lock() {
    while (!xSemaphoreTake(hal.write_lock, portMAX_DELAY));
}

static void uart_write_unlock() {
    xSemaphoreGive(hal.write_lock);
}

static void uart_read_lock() {
    while (!xSemaphoreTake(hal.read_lock, portMAX_DELAY));
}

static void uart_read_unlock() {
    xSemaphoreGive(hal.read_lock);
}

static uint8_t uart_execute(
    uint8_t command, 
    size_t args_size,
    const void* args,
    size_t *resp_size,
    void* resp
) {
    if (args_size > 0xFFFFFF) {
        ESP_LOGE(TAG, "Maximum payload size is 0xFFFFFF");
        return 0xFE;
    }

    uint32_t payload = (command << 24) | args_size;

    uart_write_lock(); // Locks: write
    
    write_all(&payload, sizeof(uint32_t));
    if (args_size > 0) {
        write_all(args, args_size);
    }

    uart_read_lock(); // Locks: write, read
    
    uint32_t result = 0;
    read_exact(&result, sizeof(uint32_t));

    uint8_t ret = (result >> 24);
    uint32_t sz = (result & 0xFFFFFF);
    uint32_t buffer_size = resp_size ? *resp_size : 0;

    if (sz > buffer_size) {
        ESP_LOGE(TAG, "Simulator returned bigger payload than buffer -- aborting");
        abort();
    }

    if (sz > 0) {
        read_exact(resp, sz);
    }
    if (resp_size) {
        *resp_size = sz;
    }

    uart_read_unlock(); // Locks: write
    uart_write_unlock(); // Locks: -
    return ret;
}


static uint8_t uart_query(uint8_t cmd) {
    uint8_t ret = uart_execute(
        cmd,
        0,
        NULL,
        NULL,
        NULL
    );

    if (ret & 0x80) {
        ESP_LOGE(TAG, "query(%u) failed: %u", cmd, ret);
    }

    return ret;
}

static uint8_t uart_do_long_poll() {
    uart_write_lock(); // Locks: -
    uart_read_lock();  // Locks: write

    // Enter long polling
    uint32_t cmd = 0xF4000000;
    write_all(&cmd, sizeof(uint32_t));  // Locks: write, read
    
    uart_write_unlock();    // Release write lock

    uint32_t result = 0;
    read_exact(&result, sizeof(uint32_t));
    uart_read_unlock();  // Got data, release read lock

    if ((result & 0x00FFFFFF) != 0) {
        ESP_LOGE(TAG, "long poll returned data -- aborting");
        abort();
    }

    return (result >> 24);
}

static void uart_polling_task() {
    static uint8_t wlan_buffer[1600];
    size_t wlan_received = 1600;

    while (1) {
        uint8_t ret = uart_do_long_poll();
        if (ret == 0) {
            // Nothing happened - wake up from another cmd
            continue;
        }
        
        switch (ret) {
            case 1:  // data at SPI interface
            {
                spi_packet_t* buffer = calloc(1, sizeof(spi_packet_t));
                buffer->len = sizeof(buffer->data);
                if (uart_execute(0x02, 0, NULL, &buffer->len, &buffer->data) == 1) {
                    xQueueSend(hal.spi_queue, &buffer, portMAX_DELAY);
                } else {
                    ESP_LOGE(TAG, "Got notified of spi packet but nothing came");
                }
            }
                break;
            case 2:  // STA arrived
                esp_event_post(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, NULL, 0, portMAX_DELAY);
                break;
            case 3:  // STA left
                esp_event_post(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, NULL, 0, portMAX_DELAY);
                break;
            case 4:  // Connected to AP
                esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, NULL, 0, portMAX_DELAY);
                break;
            case 5:  // Connection to AP lost
                esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, NULL, 0, portMAX_DELAY);
                break;
            case 6:  // data at AP interface
                wlan_received = sizeof(wlan_buffer);
                if (uart_execute(0x16, 0, NULL, &wlan_received, wlan_buffer) == 1) {
                    if (vnic_transmit(&hal.wlan.ap_tx, wlan_buffer, wlan_received) != ESP_OK) {
                        ESP_LOGE(TAG, "sta vnic transmit failed");
                    }
                } else {
                    ESP_LOGE(TAG, "wlan sta read failed");
                }
                break;
            case 7:  // data at STA interface
                wlan_received = sizeof(wlan_buffer);
                if (uart_execute(0x15, 0, NULL, &wlan_received, wlan_buffer) == 1) {
                    if (vnic_transmit(&hal.wlan.sta_tx, wlan_buffer, wlan_received) != ESP_OK) {
                        ESP_LOGE(TAG, "sta vnic transmit failed");
                    }
                } else {
                    ESP_LOGE(TAG, "wlan sta read failed");
                }
                break;
            default:
                ESP_LOGE(TAG, "Unexpected poll result: %u -- aborting", ret);
                abort();
        }
    }

    vTaskDelete(NULL);
}

static void nic_task_sta()
{
    static uint8_t buffer[1600];
    size_t recvd = 0;

    while (true)
    {
        vnic_result_t verr;
        if ((verr = vnic_receive(&hal.wlan.sta_rx, buffer, 1600, &recvd)) != VNIC_OK)
        {
            ESP_LOGE(TAG, "vnic_receive failed: %u\n", verr);
            break;
        }

        uart_execute(0x14, recvd, buffer, NULL, NULL);
    }

    vTaskDelete(NULL);
}

static void nic_task_ap()
{
    static uint8_t buffer[1600];
    size_t recvd = 0;

    while (true)
    {
        vnic_result_t verr;
        if ((verr = vnic_receive(&hal.wlan.ap_rx, buffer, 1600, &recvd)) != VNIC_OK)
        {
            ESP_LOGE(TAG, "vnic_receive failed: %u\n", verr);
            break;
        }

        uart_execute(0x17, recvd, buffer, NULL, NULL);
    }

    vTaskDelete(NULL);
}

void i4a_hal_init() {
    if (hal.initialized) {
        ESP_LOGW(TAG, "Trying to reinitialize HAL -- skipping.");
        return;
    }

    hal.read_lock = xSemaphoreCreateMutexStatic(&hal._st_read_lock);
    hal.write_lock = xSemaphoreCreateMutexStatic(&hal._st_write_lock);
    hal.spi_queue = xQueueCreate(1, sizeof(spi_packet_t*));
    setup_uart();

    xTaskCreatePinnedToCore(uart_polling_task, "uart_polling_task", 4096, NULL, 10, NULL, 1);
}

uint8_t hal_get_config_bits() {
    ESP_LOGI(TAG, "Querying board config through UART...");

    return uart_query(0x03);
}

esp_err_t hal_spi_init() {
    return ESP_OK;
}

esp_err_t hal_spi_send(const void *p, size_t len) {
    return uart_execute(0x01, (uint32_t) len, p, NULL, NULL) == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t hal_spi_recv(void *p, size_t *len) {
    spi_packet_t *spi_packet = NULL;
    while (xQueueReceive(hal.spi_queue, &spi_packet, portMAX_DELAY) != pdTRUE) ;
    
    if (spi_packet->len > *len) {
        ESP_LOGE(TAG, "buffer too small (%lu < %lu)", *len, spi_packet->len);
        abort();
    }

    *len = spi_packet->len;
    memcpy(p, spi_packet->data, spi_packet->len);
    free(spi_packet);
    return ESP_OK;
}

/** Creates netifs for AP & STA */
esp_err_t hal_wifi_init(const wifi_init_config_t *config) {
    assert(vnic_create(&hal.wlan.ap_tx) == VNIC_OK);
    assert(vnic_create(&hal.wlan.ap_rx) == VNIC_OK);
    assert(vnic_create(&hal.wlan.sta_tx) == VNIC_OK);
    assert(vnic_create(&hal.wlan.sta_rx) == VNIC_OK);

    assert(vnic_bind_receiver(&hal.wlan.ap_tx, &hal.wlan.ap_rx) == VNIC_OK);
    assert(vnic_bind_receiver(&hal.wlan.sta_tx, &hal.wlan.sta_rx) == VNIC_OK);

    uint32_t mac = esp_random();
   
    extern const esp_netif_ip_info_t _g_esp_netif_soft_ap_ip;

    esp_netif_inherent_config_t ap_config = {
        .flags = ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP,
        .mac = {0xaa, 0xaa, (mac >> 24) & 0xFF, (mac >> 16) & 0xFF, (mac >> 8) & 0xFF, mac & 0xFF},
        .ip_info = &_g_esp_netif_soft_ap_ip,
        .get_ip_event = 0,
        .lost_ip_event = 0,
        .if_key = "WIFI_AP_DEF",
        .if_desc = "Virtual WiFi AP",
        .route_prio = 10};

    esp_netif_inherent_config_t sta_config = {
        .flags = ESP_NETIF_DHCP_CLIENT | ESP_NETIF_FLAG_GARP | ESP_NETIF_FLAG_EVENT_IP_MODIFIED,
        .mac = {0xaa, 0xaa, (mac >> 24) & 0xFF, (mac >> 16) & 0xFF, (mac >> 8) & 0xFF, mac & 0xFF},
        ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(ip_info)
        .get_ip_event = IP_EVENT_STA_GOT_IP,
        .lost_ip_event = IP_EVENT_STA_LOST_IP,
        .if_key = "WIFI_STA_DEF",
        .if_desc = "Virtual WiFi STA",
        .route_prio = 100};

    assert(vnic_register_esp_netif(&hal.wlan.ap_tx, ap_config) == VNIC_OK);
    assert(vnic_register_esp_netif(&hal.wlan.sta_tx, sta_config) == VNIC_OK);
    xTaskCreate(nic_task_ap, "nic_task_ap", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(nic_task_sta, "nic_task_sta", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
    return ESP_OK;
}

esp_err_t hal_wifi_start(void) {
    ESP_LOGI(TAG, "hal_wifi_start()");
    uart_query(0x0B);
    if (hal.wlan.mode == WIFI_MODE_AP) {
        esp_event_post(WIFI_EVENT, WIFI_EVENT_AP_START, NULL, 0, portMAX_DELAY);
    }
    return ESP_OK;
}

esp_err_t hal_wifi_stop(void) {
    ESP_LOGI(TAG, "hal_wifi_stop()");
    uart_query(0x0C);
    return ESP_OK;
}

esp_err_t hal_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf) {
    if (interface == WIFI_IF_AP) {
        ESP_LOGI(
            TAG, 
            "hal_wifi_set_config(ap, ssid=%s, password=%s, channel=%u)", 
            conf->ap.ssid, conf->ap.password, conf->ap.channel
        );
        struct {
            char ssid[32];
            char password[64];
            uint32_t channel;
        } payload = {0};
        strcpy(payload.ssid, (char *)conf->ap.ssid);
        strcpy(payload.password, (char *)conf->ap.password);
        payload.channel = conf->ap.channel;
        return uart_execute(0x06, sizeof(payload), &payload, NULL, NULL) == 0 ? ESP_OK : ESP_FAIL;
    } else if (interface == WIFI_IF_STA) {
        ESP_LOGI(
            TAG, 
            "hal_wifi_set_config(sta, ssid=%s, password=%s, bssid_set=%u, bssid=%02x:%02x:%02x:%02x:%02x:%02x)", 
            conf->sta.ssid, conf->sta.password, conf->sta.bssid_set, 
            conf->sta.bssid[0], conf->sta.bssid[1], conf->sta.bssid[2], conf->sta.bssid[3], 
            conf->sta.bssid[4], conf->sta.bssid[5]
        );
        struct {
            char ssid[32];
            char password[64];
        } payload = {0};
        strcpy(payload.ssid, (char *)conf->sta.ssid);
        strcpy(payload.password, (char *)conf->sta.password);
        return uart_execute(0x07, sizeof(payload), &payload, NULL, NULL) == 0 ? ESP_OK : ESP_FAIL;
    }

    return ESP_ERR_WIFI_NOT_INIT;
}

esp_err_t hal_wifi_set_mode(wifi_mode_t mode) {
    ESP_LOGI(TAG, "hal_wifi_set_mode(%u)", mode);
    if ((mode != WIFI_MODE_AP) && (mode != WIFI_MODE_STA)) {
        ESP_LOGE(TAG, "WiFi mode %u not supported by emulator", mode);
        return ESP_FAIL;
    }

    hal.wlan.mode = mode;
    uint32_t mode_u32 =(uint32_t)mode;
    return uart_execute(0x05, sizeof(mode_u32), &mode_u32, NULL, NULL) == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t hal_wifi_connect(void) {
    ESP_LOGI(TAG, "hal_wifi_connect()");
    uart_query(0x08);  // result == connected?
    return ESP_OK;
}

esp_err_t hal_wifi_disconnect(void) {
    ESP_LOGI(TAG, "hal_wifi_disconnect()");
    uart_query(0x09);
    return ESP_OK;
}

esp_err_t hal_wifi_deauth_sta(uint16_t aid) {
    ESP_LOGI(TAG, "hal_wifi_deauth_sta(%u)", aid);
    return uart_execute(0x0A, sizeof(aid), &aid, NULL, NULL) == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t hal_wifi_scan_start(const wifi_scan_config_t *config, bool block) {
    if (!block) {
        ESP_LOGE(TAG, "non blocking scan not implemented");
        return ESP_FAIL;
    }

    if (config) {
        ESP_LOGE(TAG, "scan with custom config not implemented");
        return ESP_FAIL;
    }

    return uart_query(0x10) == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t hal_wifi_scan_get_ap_num(uint16_t *number) {
    uint8_t ret = uart_query(0x0D);
    if (ret & 0x80)
        return ESP_FAIL;
    
    *number = (uint16_t) ret;
    return ESP_OK;
}

esp_err_t hal_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records) {
    uint16_t result_len = *number;
    uint16_t n_aps = uart_query(0x0D);
    if (n_aps & 0x80) {
        ESP_LOGE(TAG, "query(0x0D) failed: %u", n_aps);
        return ESP_FAIL;
    }

    struct {
        uint8_t bssid[6];
        uint8_t ssid[33];
        uint8_t primary;
        int8_t  rssi;
    } __attribute__((packed)) record;

    for (uint16_t i = 0; i < n_aps; i++) {
        size_t record_size = sizeof(record);
        uart_execute(0x0F, 0, NULL, &record_size, (uint8_t*)&record);

        if (i < result_len) {
            memcpy(ap_records[i].bssid, record.bssid, sizeof(ap_records[i].bssid));
            memcpy(ap_records[i].ssid, record.ssid, sizeof(ap_records[i].ssid));
            ap_records[i].primary = record.primary;
            ap_records[i].rssi = record.rssi;
        }
    }

    *number = n_aps;
    return ESP_OK;
}

esp_err_t hal_wifi_ap_get_sta_list(wifi_sta_list_t *sta) {
    uint8_t n_stas = uart_query(0x12);
    if (n_stas & 0x80) {
        ESP_LOGE(TAG, "Query(0x12) failed: %u", n_stas);
        return ESP_FAIL;
    }

    if (n_stas > ESP_WIFI_MAX_CONN_NUM) {
        ESP_LOGW(TAG, "More stations than allowed by esp_wifi -- ignoring some");
    }

    sta->num = 0;

    struct {
        uint8_t mac[6];
        int8_t  rssi;
    } __attribute__((packed)) record;

    for (uint32_t i = 0; i < n_stas; i++) {
        size_t record_size = sizeof(record);
        uart_execute(0x13, sizeof(uint32_t), &i, &record_size, (uint8_t*)&record);

        if (sta->num < ESP_WIFI_MAX_CONN_NUM) {
            memcpy(sta->sta[sta->num].mac, record.mac, sizeof(sta->sta[sta->num].mac));
            sta->sta[sta->num].rssi = record.rssi;
            sta->num++;
        }
    }

    return ESP_OK;
}

esp_err_t hal_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info) {
    struct {
        uint8_t bssid[6];
        uint8_t ssid[33];
        uint8_t primary;
        int8_t  rssi;
    } __attribute__((packed)) record;

    size_t record_size = sizeof(record);
    uint32_t ret = uart_execute(0x11, 0, NULL, &record_size, &record);

    if (ret != 0) {
        ESP_LOGE(TAG, "hal_wifi_sta_get_ap_info failed: %u", ret);
        return ESP_FAIL;
    }

    memcpy(ap_info->bssid, record.bssid, sizeof(ap_info->bssid));
    memcpy(ap_info->ssid, record.ssid, sizeof(ap_info->ssid));
    ap_info->primary = record.primary;
    ap_info->rssi = record.rssi;

    return ESP_OK;
}

esp_netif_t* hal_netif_create_default_wifi_ap() {
    return esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
}

esp_netif_t* hal_netif_create_default_wifi_sta() {
    return esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
}
