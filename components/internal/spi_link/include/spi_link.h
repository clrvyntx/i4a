#ifndef _SPI_LINK_H_
#define _SPI_LINK_H_

#include "esp_netif.h"

#define SPI_DEVICE_IP(orientation) ESP_IP4TOADDR(172, 16, 1, (orientation))
#define SPI_NETWORK ESP_IP4TOADDR(172, 16, 1, 0)
#define SPI_NETWORK_MASK ESP_IP4TOADDR(255, 255, 255, 0)
#define SPI_IF_KEY "SPI_TX"

#define get_spi_link_tx_netif() esp_netif_get_handle_from_ifkey(SPI_IF_KEY)

esp_err_t spi_link_init(uint32_t orientation);
bool broadcast_to_siblings(const uint8_t *msg, uint16_t len);

#endif // _SPI_LINK_H_
