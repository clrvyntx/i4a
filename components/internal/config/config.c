#include "config.h"

#include "i4a_hal.h"

static const char* TAG = "==> config";

static config_t s_config = {
    .id          = CONFIG_ID_NONE,
    .mode        = CONFIG_MODE_NONE,
    .orientation = CONFIG_ORIENTATION_NONE,
    .rx_ip_addr  = 0,
    .tx_ip_addr  = 0,
};


// Set the corresponding bits in config_bits based on GPIO pin values
static uint8_t read_config_bits(void)
{
    ESP_LOGI(TAG, "Querying board config through UART...");
    uint8_t config_bits = hal_get_config_bits();

    ESP_LOGI(TAG, "Raw pin values: %d %d %d", 
         config_bits & 0b001,
         config_bits & 0b010,
         config_bits & 0b100);
    return config_bits;
}

// Set device config based on config_bits values
void config_setup(void)
{
    i4a_hal_init();
    uint8_t config_bits = read_config_bits();
    ESP_LOGI(TAG, "config_bits: %d (binario: %d%d%d)", 
         config_bits,
         (config_bits >> 2) & 1,
         (config_bits >> 1) & 1,
         config_bits & 1);
    s_config.id = (config_id_t) config_bits;

    if ((config_bits >> 2) == 0) {
        s_config.mode = CONFIG_MODE_PEER_LINK;
        s_config.orientation = (config_orientation_t) config_bits;
    } else {
        s_config.mode = (config_mode_t) config_bits;
        s_config.orientation = CONFIG_ORIENTATION_NONE;
    }
    s_config.rx_ip_addr = ESP_IP4TOADDR(192, 168, 0, (int)(s_config.orientation) + 1);
    s_config.tx_ip_addr = ESP_IP4TOADDR(192, 168, 0, (int)(s_config.orientation) + 10);
}

config_id_t config_get_id(void)
{
    return s_config.id;
}

esp_netif_ip_info_t config_get_rx_ip_info(void)
{
    esp_netif_ip_info_t ip_info = {
        .ip = {.addr = s_config.rx_ip_addr},
        .gw = {.addr = s_config.tx_ip_addr},
        .netmask = {.addr = ESP_IP4TOADDR(255, 255, 255, 255)},
    };
    return ip_info;
}

esp_netif_ip_info_t config_get_tx_ip_info(void)
{
    esp_netif_ip_info_t ip_info = {
        .ip = {.addr = s_config.tx_ip_addr},
        .gw = {.addr = s_config.tx_ip_addr},
        .netmask = {.addr = ESP_IP4TOADDR(255, 255, 0, 0)},
    };
    return ip_info;
}

void config_print(void)
{
    ESP_LOGI(TAG, "=============== Board Config ===============");
    ESP_LOGI(TAG, "Board ID: '%i'", s_config.id);
    ESP_LOGI(TAG, "Board Mode: '%i'", s_config.mode);
    ESP_LOGI(TAG, "Board Orientation: '%i'", s_config.orientation);
    ESP_LOGI(TAG, "============================================");
}

bool config_mode_is(config_mode_t mode)
{
    return s_config.mode == mode;
}

bool config_orientation_is(config_orientation_t orientation)
{
    return s_config.orientation == orientation;
}
