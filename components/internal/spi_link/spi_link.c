#include "esp_log.h"

#include "spi_link.h"
#include "spi_hal.h"
#include "broadcast.h"
#include "netif_glue.h"

#define TAG "spi_nic"

esp_err_t spi_link_init(uint32_t orientation) {
    orientation++;
    if (orientation > 4)
        orientation = 5;  // Orientation used for IP, ensure consecutive and != 0
    
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK)
        return err;
    
    err = hal_spi_init();
    if (err != ESP_OK)
        return err;
    
    const esp_netif_ip_info_t ip_tx = {
        .ip = {.addr = SPI_DEVICE_IP(orientation)},
        .gw = {.addr = SPI_DEVICE_IP((orientation % 5) + 1)},
        .netmask = {.addr = SPI_NETWORK_MASK}};

    err = create_default_spi_netif(ip_tx);
    if (err != ESP_OK)
        return err;
    
    return setup_spi_communication(orientation) ? ESP_OK : ESP_FAIL;
}
