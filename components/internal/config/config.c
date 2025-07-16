#include "config.h"

static const char* TAG = "==> config";

static config_t s_config = {
    .id          = CONFIG_ID_NONE,
    .mode        = CONFIG_MODE_NONE,
    .orientation = CONFIG_ORIENTATION_NONE,
    .rx_ip_addr  = 0,
    .tx_ip_addr  = 0,
};

static void enable_config_pins(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = CONFIG_PIN_MASK,
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

static void reset_config_pins(void)
{
    gpio_reset_pin(CONFIG_PIN_0);
    gpio_reset_pin(CONFIG_PIN_1);
    gpio_reset_pin(CONFIG_PIN_2);
}

// Set the corresponding bits in config_bits based on GPIO pin values
static uint8_t read_config_bits(void)
{
    // Initialize config_bits with all zeros
    uint8_t config_bits = 0b0;

    enable_config_pins();
    config_bits |= ((~gpio_get_level(CONFIG_PIN_0)) & 0b00000001) << 0;
    config_bits |= ((~gpio_get_level(CONFIG_PIN_1)) & 0b00000001) << 1;
    config_bits |= ((~gpio_get_level(CONFIG_PIN_2)) & 0b00000001) << 2;
    ESP_LOGI(TAG, "Raw pin values: %d %d %d", 
         gpio_get_level(CONFIG_PIN_0),
         gpio_get_level(CONFIG_PIN_1),
         gpio_get_level(CONFIG_PIN_2));
    reset_config_pins();

    return config_bits;
}

// Set device config based on config_bits values
void config_setup(void)
{
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
