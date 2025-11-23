#include "i4a_hal.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "virtual_nic.h"

#include "uart.h"

const char *TAG = "i4a_hal";

static SemaphoreHandle_t s_uart_lock = NULL;
static SemaphoreHandle_t s_uart_read_lock = NULL;
static SemaphoreHandle_t s_uart_write_lock = NULL;

static inline void uart_execute(uint32_t cmd, uint32_t payload_size, const void *payload) {
    while (!xSemaphoreTake(s_uart_write_lock, portMAX_DELAY));
    write_all(&cmd, sizeof(uint32_t));
    write_all(&payload_size, sizeof(uint32_t));
    if (payload_size > 0) {
        write_all(payload, payload_size);
    }
    xSemaphoreGive(s_uart_write_lock);
}

static inline uint32_t uart_query(uint32_t cmd) {
    uart_execute(cmd, 0, NULL);

    while (!xSemaphoreTake(s_uart_read_lock, portMAX_DELAY));
    uint32_t result = 0;
    read_exact(&result, sizeof(uint32_t));
    xSemaphoreGive(s_uart_read_lock);
    return result;
}

void i4a_hal_init() {
    s_uart_lock = xSemaphoreCreateMutex();
    s_uart_read_lock = xSemaphoreCreateMutex();
    s_uart_write_lock = xSemaphoreCreateMutex();
    if(!s_uart_lock || !s_uart_read_lock || !s_uart_write_lock) {
        ESP_LOGE(TAG, "cant create mutex");
        abort();
    }

    setup_uart();
}

uint32_t hal_get_config_bits() {
    while (!xSemaphoreTake(s_uart_lock, portMAX_DELAY));
    uint32_t ret = uart_query(0x03);
    xSemaphoreGive(s_uart_lock);
    return ret;
}

esp_err_t hal_spi_init() {
    return ESP_OK;
}

esp_err_t hal_spi_send(const void *p, size_t len) {
    while (!xSemaphoreTake(s_uart_lock, portMAX_DELAY));
    uart_execute(0x01, (uint32_t) len, p);
    xSemaphoreGive(s_uart_lock);
    return ESP_OK;
}

static esp_err_t _hal_spi_recv(void *p, size_t *len) {
    uint32_t payload_size = uart_query(0x02);
    if (!payload_size) {
        return ESP_ERR_NOT_FOUND;
    }

    if (payload_size > *len) {
        ESP_LOGE(TAG, "Got payload bigger than buffer size (%zu bytes) -- abort", payload_size);
        abort();
    }
    
    *len = payload_size;
    read_exact(p, payload_size);
    return ESP_OK;
}

esp_err_t hal_spi_recv(void *p, size_t *len) {
    // Enter long poll
    while (!xSemaphoreTake(s_uart_lock, portMAX_DELAY));  // -- cant use any other function
    while (!xSemaphoreTake(s_uart_read_lock, portMAX_DELAY));  // -- read exclusive for us
    uart_execute(0x04, 0, NULL); // enter long poll
    xSemaphoreGive(s_uart_lock); // allow execution of other commands, but still hold read lock

    uint32_t result = 0;
    read_exact(&result, sizeof(uint32_t));
    xSemaphoreGive(s_uart_read_lock); // -- long poll finished, return read lock

    while (!xSemaphoreTake(s_uart_lock, portMAX_DELAY)); // -- regrab uart lock and do regular spi read
    esp_err_t ret = _hal_spi_recv(p, len);
    xSemaphoreGive(s_uart_lock);
    return ret;
}

static struct wlan_vnic {
    vnic_t ap_tx, ap_rx;
    vnic_t sta_tx, sta_rx;
    esp_netif_t *ap_netif, *sta_netif;
} wlan;

/** Creates netifs for AP & STA */
void hal_wifi_init() {
    assert(vnic_create(&wlan.ap_tx) == VNIC_OK);
    assert(vnic_create(&wlan.ap_rx) == VNIC_OK);
    assert(vnic_create(&wlan.sta_tx) == VNIC_OK);
    assert(vnic_create(&wlan.sta_rx) == VNIC_OK);

    assert(vnic_bind_receiver(&wlan.ap_tx, &wlan.ap_rx) == VNIC_OK);
    assert(vnic_bind_receiver(&wlan.sta_tx, &wlan.sta_rx) == VNIC_OK);

    esp_netif_ip_info_t null_ip = {0};
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
        .flags =ESP_NETIF_DHCP_CLIENT | ESP_NETIF_FLAG_GARP | ESP_NETIF_FLAG_EVENT_IP_MODIFIED,
        .mac = {0xaa, 0xaa, (mac >> 24) & 0xFF, (mac >> 16) & 0xFF, (mac >> 8) & 0xFF, mac & 0xFF},
        ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(ip_info)
        .get_ip_event = IP_EVENT_STA_GOT_IP,
        .lost_ip_event = IP_EVENT_STA_LOST_IP,
        .if_key = "WIFI_STA_DEF",
        .if_desc = "Virtual WiFi STA",
        .route_prio = 100};

    assert(vnic_register_esp_netif(&wlan.ap_tx, ap_config) == VNIC_OK);
    assert(vnic_register_esp_netif(&wlan.sta_tx, sta_config) == VNIC_OK);
}

esp_err_t hal_wifi_set_mode(wifi_mode_t mode) {
    return ESP_OK;
}

esp_netif_t* hal_netif_create_default_wifi_ap() {
    return esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
}

esp_netif_t* hal_netif_create_default_wifi_sta() {
    return esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
}