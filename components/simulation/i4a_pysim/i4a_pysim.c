#include "i4a_pysim.h"
#include "pysim.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "virtual_nic.h"


#define TAG "i4a_pysim"

typedef struct {
    size_t len;
    uint8_t data[1600];
} spi_packet_t;

static struct {
    bool initialized;

    StaticSemaphore_t _st_spi_queue;
    QueueHandle_t spi_queue;

    struct {
        vnic_t ap_tx, ap_rx;
        vnic_t sta_tx, sta_rx;
        esp_netif_t *ap_netif, *sta_netif;
        wifi_mode_t mode;
    } wlan;
} hal = { 0 };

static void event_spi_rx(uint8_t event_id, const void *event_data, size_t sz_event_data) {
    spi_packet_t* buffer = calloc(1, sizeof(spi_packet_t));
    buffer->len = sz_event_data;
    memcpy(buffer->data, event_data, sz_event_data);
    xQueueSend(hal.spi_queue, &buffer, portMAX_DELAY);
}

static void event_sta_arrived(uint8_t event_id, const void *event_data, size_t sz_event_data) {
    esp_event_post(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, NULL, 0, portMAX_DELAY);
}

static void event_sta_left(uint8_t event_id, const void *event_data, size_t sz_event_data) {
    esp_event_post(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, NULL, 0, portMAX_DELAY);
}

static void event_connected_to_ap(uint8_t event_id, const void *event_data, size_t sz_event_data) {
    esp_netif_action_connected(
        esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"),
        WIFI_EVENT,
        WIFI_EVENT_STA_CONNECTED,
        NULL
    );
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, NULL, 0, portMAX_DELAY);
}

static void event_connection_to_ap_lost(uint8_t event_id, const void *event_data, size_t sz_event_data) {
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, NULL, 0, portMAX_DELAY);
}

static void event_wlan_ap_rx(uint8_t event_id, const void *event_data, size_t sz_event_data) {
    if (vnic_transmit(&hal.wlan.ap_rx, event_data, sz_event_data) != VNIC_OK) {
        ESP_LOGE(TAG, "sta vnic transmit failed");
    }
}

static void event_wlan_sta_rx(uint8_t event_id, const void *event_data, size_t sz_event_data) {
    if (vnic_transmit(&hal.wlan.sta_rx, event_data, sz_event_data) != VNIC_OK) {
        ESP_LOGE(TAG, "sta vnic transmit failed");
    }
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

        ps_execute(0x14, buffer, recvd, NULL, NULL);
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

        ps_execute(0x17, buffer, recvd, NULL, NULL);
    }

    vTaskDelete(NULL);
}

static esp_err_t _ps_wifi_init() {
    assert(vnic_create(&hal.wlan.ap_tx) == VNIC_OK);
    assert(vnic_create(&hal.wlan.ap_rx) == VNIC_OK);
    assert(vnic_create(&hal.wlan.sta_tx) == VNIC_OK);
    assert(vnic_create(&hal.wlan.sta_rx) == VNIC_OK);

    assert(vnic_bind_receiver(&hal.wlan.ap_tx, &hal.wlan.ap_rx) == VNIC_OK);
    assert(vnic_bind_receiver(&hal.wlan.sta_tx, &hal.wlan.sta_rx) == VNIC_OK);
    assert(vnic_bind_receiver(&hal.wlan.ap_rx, &hal.wlan.ap_tx) == VNIC_OK);
    assert(vnic_bind_receiver(&hal.wlan.sta_rx, &hal.wlan.sta_tx) == VNIC_OK);

    esp_netif_config_t ap_config = ESP_NETIF_DEFAULT_WIFI_AP();
    esp_netif_config_t sta_config = ESP_NETIF_DEFAULT_WIFI_STA();

    assert(vnic_register_esp_netif(&hal.wlan.ap_tx, ap_config) == VNIC_OK);
    assert(vnic_register_esp_netif(&hal.wlan.sta_tx, sta_config) == VNIC_OK);
    xTaskCreate(nic_task_ap, "nic_task_ap", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(nic_task_sta, "nic_task_sta", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);

    uint32_t mac = esp_random();
    uint8_t wl_mac[6] = {0xaa, 0xaa, (mac >> 24) & 0xFF, (mac >> 16) & 0xFF, (mac >> 8) & 0xFF, mac & 0xFF};
    esp_netif_t *ap = ps_netif_create_default_wifi_ap();
    esp_netif_set_mac(ap, wl_mac);
    esp_netif_t *sta = ps_netif_create_default_wifi_sta();
    esp_netif_set_mac(sta, wl_mac);
    return ESP_OK;
}

void i4a_pysim_init() {
    if (hal.initialized) {
        ESP_LOGW(TAG, "Trying to reinitialize HAL -- skipping.");
        return;
    }

    ps_register_event(0x01, event_spi_rx);
    ps_register_event(0x02, event_sta_arrived);
    ps_register_event(0x03, event_sta_left);
    ps_register_event(0x04, event_connected_to_ap);
    ps_register_event(0x05, event_connection_to_ap_lost);
    ps_register_event(0x06, event_wlan_ap_rx);
    ps_register_event(0x07, event_wlan_sta_rx);

    hal.spi_queue = xQueueCreate(1, sizeof(spi_packet_t*));
    _ps_wifi_init();

    pysim_start();
}

uint8_t ps_get_config_bits() {
    ESP_LOGI(TAG, "Querying board config through UART...");

    return ps_query(0x03);
}

esp_err_t ps_spi_init() {
    return ESP_OK;
}

esp_err_t ps_spi_send(const void *p, size_t len) {
    return ps_execute(0x01, p, len, NULL, NULL) == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t ps_spi_recv(void *p, size_t *len) {
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
esp_err_t ps_wifi_init(const wifi_init_config_t *config) {
    return ESP_OK;
}

esp_err_t ps_wifi_start(void) {
    ESP_LOGI(TAG, "ps_wifi_start()");
    ps_query(0x0B);
    if (hal.wlan.mode == WIFI_MODE_AP) {
        esp_event_post(WIFI_EVENT, WIFI_EVENT_AP_START, NULL, 0, portMAX_DELAY);
    } else if (hal.wlan.mode == WIFI_MODE_STA) {
        esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_START, NULL, 0, portMAX_DELAY);
    } else if (hal.wlan.mode == WIFI_MODE_APSTA) {
        esp_event_post(WIFI_EVENT, WIFI_EVENT_AP_START, NULL, 0, portMAX_DELAY);
        esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_START, NULL, 0, portMAX_DELAY);
    }
    return ESP_OK;
}

esp_err_t ps_wifi_stop(void) {
    ESP_LOGI(TAG, "ps_wifi_stop()");
    ps_query(0x0C);
    return ESP_OK;
}

esp_err_t ps_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf) {
    if (interface == WIFI_IF_AP) {
        ESP_LOGI(
            TAG, 
            "ps_wifi_set_config(ap, ssid=%s, password=%s, channel=%u)", 
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
        return ps_execute(0x06, &payload, sizeof(payload), NULL, NULL) == 0 ? ESP_OK : ESP_FAIL;
    } else if (interface == WIFI_IF_STA) {
        ESP_LOGI(
            TAG, 
            "ps_wifi_set_config(sta, ssid=%s, password=%s, bssid_set=%u, bssid=%02x:%02x:%02x:%02x:%02x:%02x)", 
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
        return ps_execute(0x07, &payload, sizeof(payload), NULL, NULL) == 0 ? ESP_OK : ESP_FAIL;
    }

    return ESP_ERR_WIFI_NOT_INIT;
}

esp_err_t ps_wifi_set_mode(wifi_mode_t mode) {
    ESP_LOGI(TAG, "ps_wifi_set_mode(%u)", mode);
    if ((mode != WIFI_MODE_AP) && (mode != WIFI_MODE_STA) && (mode != WIFI_MODE_APSTA)) {
        ESP_LOGE(TAG, "WiFi mode %u not supported by emulator", mode);
        return ESP_FAIL;
    }

    hal.wlan.mode = mode;
    uint32_t mode_u32 =(uint32_t)mode;
    return ps_execute(0x05, &mode_u32, sizeof(mode_u32), NULL, NULL) == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t ps_wifi_connect(void) {
    ESP_LOGI(TAG, "ps_wifi_connect()");
    ps_query(0x08);  // result == connected?
    return ESP_OK;
}

esp_err_t ps_wifi_disconnect(void) {
    ESP_LOGI(TAG, "ps_wifi_disconnect()");
    ps_query(0x09);
    return ESP_OK;
}

esp_err_t ps_wifi_deauth_sta(uint16_t aid) {
    ESP_LOGI(TAG, "ps_wifi_deauth_sta(%u)", aid);
    return ps_execute(0x0A, &aid, sizeof(aid), NULL, NULL) == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t ps_wifi_scan_start(const wifi_scan_config_t *config, bool block) {
    if (!block) {
        ESP_LOGE(TAG, "non blocking scan not implemented");
        return ESP_FAIL;
    }

    if (config) {
        ESP_LOGE(TAG, "scan with custom config not implemented");
        return ESP_FAIL;
    }

    return ps_query(0x10) == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t ps_wifi_scan_get_ap_num(uint16_t *number) {
    uint8_t ret = ps_query(0x0D);
    if (ret & 0x80)
        return ESP_FAIL;
    
    *number = (uint16_t) ret;
    return ESP_OK;
}

esp_err_t ps_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records) {
    uint16_t result_len = *number;
    uint16_t n_aps = ps_query(0x0D);
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
        ps_execute(0x0F, NULL, 0, (uint8_t*)&record, &record_size);

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

esp_err_t ps_wifi_ap_get_sta_list(wifi_sta_list_t *sta) {
    uint8_t n_stas = ps_query(0x12);
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
        ps_execute(0x13, &i, sizeof(uint32_t), (uint8_t*)&record, &record_size);

        if (sta->num < ESP_WIFI_MAX_CONN_NUM) {
            memcpy(sta->sta[sta->num].mac, record.mac, sizeof(sta->sta[sta->num].mac));
            sta->sta[sta->num].rssi = record.rssi;
            sta->num++;
        }
    }

    return ESP_OK;
}

esp_err_t ps_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info) {
    struct {
        uint8_t bssid[6];
        uint8_t ssid[33];
        uint8_t primary;
        int8_t  rssi;
    } __attribute__((packed)) record;

    size_t record_size = sizeof(record);
    uint32_t ret = ps_execute(0x11, NULL, 0, &record, &record_size);

    if (ret != 0) {
        ESP_LOGE(TAG, "ps_wifi_sta_get_ap_info failed: %u", ret);
        return ESP_FAIL;
    }

    memcpy(ap_info->bssid, record.bssid, sizeof(ap_info->bssid));
    memcpy(ap_info->ssid, record.ssid, sizeof(ap_info->ssid));
    ap_info->primary = record.primary;
    ap_info->rssi = record.rssi;

    return ESP_OK;
}

esp_err_t ps_wifi_set_max_tx_power(int8_t) {
    return ESP_OK;
}

esp_netif_t* ps_netif_create_default_wifi_ap() {
    return esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
}

esp_netif_t* ps_netif_create_default_wifi_sta() {
    return esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
}

esp_err_t ps_netif_destroy_default_wifi(esp_netif_t*) {
    return ESP_OK;
}