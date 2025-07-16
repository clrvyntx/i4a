#include "wifi.h"

static const char *WIFI_TAG = "==> wifi-if";
esp_netif_t *wifi_netif;

esp_netif_t * get_wifi_netif(void){
    return wifi_netif;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base != WIFI_EVENT) {
        return;
    }
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case WIFI_EVENT_AP_STACONNECTED:
        wifi_event_ap_staconnected_t *event_c = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(WIFI_TAG, "station " MACSTR " join, AID=%d", MAC2STR(event_c->mac), event_c->aid);
        break;
    case WIFI_EVENT_AP_STADISCONNECTED:
        wifi_event_ap_stadisconnected_t *event_d = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(WIFI_TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event_d->mac), event_d->aid);
        break;
    case WIFI_EVENT_AP_START:
        break;
    default:
        break;
    }
}

esp_err_t wifi_init(void) {
    /* Initialize WiFi Allocate resource for WiFi driver, such as WiFi control structure, 
    RX/TX buffer, WiFi NVS structure etc. This WiFi also starts WiFi task. */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    return ESP_OK;
}

esp_err_t wifi_netif_init(void)
{
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
#ifdef CONFIG_WIFI_TEST_MODE
    if (config_mode_is(CONFIG_MODE_ACCESS_POINT) || config_orientation_is(CONFIG_ORIENTATION_NORTH)) {
        ESP_LOGI(WIFI_TAG, "TEST_MODE: wifi_netif_init: I'm AP! Configuring as AP.");
        wifi_ap_netif_init();
    } else {
        ESP_LOGI(WIFI_TAG, "TEST_MODE: wifi_netif_init: I'm not root! I'm gonna be a STAr!");
        wifi_sta_netif_init();
    }
#endif

#ifdef CONFIG_WIFI_NORMAL_MODE
    if (config_mode_is(CONFIG_MODE_ACCESS_POINT)) {
        ESP_LOGI(WIFI_TAG, "wifi_netif_init: I'm AP! Configuring as AP.");
        wifi_ap_netif_init();
    } else {
        ESP_LOGI(WIFI_TAG, "wifi_netif_init: I'm not root! I'm gonna be a STAr!");
        wifi_sta_netif_init();
    }
#endif
    return esp_wifi_start();
}
void wifi_sta_netif_init(void) {
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_WIFI_STA();
    wifi_netif = esp_netif_new(&cfg);
    assert(wifi_netif);
    
    ESP_ERROR_CHECK(esp_netif_attach_wifi_station(wifi_netif));
    ESP_ERROR_CHECK(esp_wifi_set_default_wifi_sta_handlers());

    wifi_config_t wifi_sta_config = {
        .sta = {
            .ssid = WIFI_AP_SSID,
            .password = WIFI_AP_PASS,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .failure_retry_cnt = 3,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    wifi_sta_config.sta.threshold.authmode = strlen(WIFI_AP_PASS) == 0? WIFI_AUTH_OPEN : WIFI_AUTH_WPA_WPA2_PSK;

    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error setting Wi-Fi mode to STA.");
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config) );

    ESP_LOGI(WIFI_TAG, "wifi_sta_netif_init finished.");
}

uint8_t get_wifi_channel() {
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    if (ret != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error getting MAC address");
        return ret;
    }
    return (mac[5] % 11) + 1; // Canales de 1 a 11
}

esp_err_t get_wifi_ip_info(esp_netif_ip_info_t *ip_info) {
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    if (ret != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error getting MAC address");
        return ret;
    }

    uint8_t last_octet = mac[5];
    ip_info->ip.addr = ESP_IP4TOADDR(CONFIG_WIFI_AP_IP_FIRST_OCTET, 168, config_get_id() + 1, 1);
    ip_info->gw.addr = ESP_IP4TOADDR(CONFIG_WIFI_AP_IP_FIRST_OCTET, 168, config_get_id() + 1, 1);
    ip_info->netmask.addr = ESP_IP4TOADDR(CONFIG_WIFI_AP_NETMASK_FIRST_OCTET, 
                                          CONFIG_WIFI_AP_NETMASK_SECOND_OCTET, 
                                          CONFIG_WIFI_AP_NETMASK_THIRD_OCTET, 
                                          CONFIG_WIFI_AP_NETMASK_FOURTH_OCTET);

    return ESP_OK;
}

esp_err_t set_wifi_ip(esp_netif_t *wifi_netif, const char *ap_name_prefix, const char *password, uint8_t max_connections) {
    esp_netif_ip_info_t ip_info;
    esp_err_t ret = get_wifi_ip_info(&ip_info);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_ERROR_CHECK(esp_netif_dhcps_stop(wifi_netif));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(wifi_netif, &ip_info));
    
    ESP_ERROR_CHECK(esp_netif_dhcps_start(wifi_netif));

    wifi_config_t wifi_config = {0};
    
    char unique_ssid[32];
    uint8_t mac[6];
    ret = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    if (ret != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error getting MAC address");
        return ret;
    }

    snprintf(unique_ssid, sizeof(unique_ssid), "%s_%02X%02X%02X", ap_name_prefix, mac[3], mac[4], mac[5]);
    strncpy((char *)wifi_config.ap.ssid, unique_ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid[sizeof(wifi_config.ap.ssid) - 1] = '\0'; // Asegurar terminación nula
    wifi_config.ap.ssid_len = strlen((char *)wifi_config.ap.ssid);
    
    strncpy((char *)wifi_config.ap.password, password, sizeof(wifi_config.ap.password));
    wifi_config.ap.password[sizeof(wifi_config.ap.password) - 1] = '\0'; // Asegurar terminación nula
    
    wifi_config.ap.max_connection = max_connections;
    wifi_config.ap.authmode = strlen(password) == 0 ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA_WPA2_PSK;
    
    wifi_config.ap.channel = get_wifi_channel();

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

    ESP_LOGI(WIFI_TAG, "WiFi AP IP set to: " IPSTR, IP2STR(&ip_info.ip));
    ESP_LOGI(WIFI_TAG, "WiFi AP SSID: %s, Channel: %d", unique_ssid, wifi_config.ap.channel);
    return ESP_OK;
}

void wifi_ap_netif_init(void) {
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_WIFI_AP();
    wifi_netif = esp_netif_new(&cfg);
    assert(wifi_netif);

    ESP_ERROR_CHECK(esp_netif_attach_wifi_ap(wifi_netif));
    ESP_ERROR_CHECK(esp_wifi_set_default_wifi_ap_handlers());

    ESP_ERROR_CHECK(set_wifi_ip(wifi_netif, "ESP32_AP", WIFI_AP_PASS, WIFI_AP_MAX_STA_CONN));

    ESP_LOGI(WIFI_TAG, "wifi_ap_netif_init finished.");
}

esp_ip4_addr_t get_wifi_ip_interface_address(void){
    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(wifi_netif, &ip_info));
    return ip_info.ip;
}

