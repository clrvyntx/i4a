// C libraries
#include <stdio.h>

// ESP32 libraries
#include "esp_event.h"
#include "esp_check.h"

// Private components
#include "config.h"
#include "ring_link.h"
#include "wifi.h"
#include "route.h"
#include "heartbeat.h"

#include "nvs.h"

// Test files
#include "test_spi.h"

#define TEST_ALL false

static const char *TAG = "==> main";


void app_main(void)
{
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());
    config_setup();
    config_print();

    ESP_ERROR_CHECK(ring_link_init());
    test_spi_run_all();

    #ifdef CONFIG_RING_LINK_LOWLEVEL_IMPL_PHYSIM
    ESP_LOGI(TAG, "CONFIG_RING_LINK_LOWLEVEL_IMPL_PHYSIM\n");
    #endif

    #ifdef CONFIG_RING_LINK_LOWLEVEL_IMPL_SPI
    ESP_LOGI(TAG, "CONFIG_RING_LINK_LOWLEVEL_IMPL_SPI\n");
    ESP_ERROR_CHECK(wifi_init());
    ESP_ERROR_CHECK(wifi_netif_init());
    #endif

    print_route_table();
    
    ESP_ERROR_CHECK(heartbeat_init());
}
