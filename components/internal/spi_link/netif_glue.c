#include "lwip/esp_netif_net_stack.h"
#include "lwip/esp_pbuf_ref.h"
#include "netif/etharp.h"
#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "spi_link.h"
#include "spi_hal.h"

#define RX_TASK_STACK_SIZE 4096
#define SPI_MAX_PAYLOAD_SIZE 1600

#define IF_NAME(x) esp_netif_get_ifkey(esp_netif_get_handle_from_netif_impl(x))
#define TAG "spi_nic"
#define TAG_LWIP TAG "_lwip"

#define SPI_MAC_ADDR                      \
    {                                      \
        0xaa, 0xaa, 0x34, 0x56, 0x78, 0x9b \
    }

typedef struct spi_nic_driver
{
    esp_netif_driver_base_t base; /*!< base structure reserved as esp-netif driver */

    // Custom fields
    esp_netif_t *spi_netif;
} spi_nic_driver_t;

static esp_err_t spi_netif_transmit(void *h, void *buffer, size_t len)
{
    // unused but required
    return ESP_OK;
}

static void spi_netif_free_rx_buffer(void *h, void *buffer)
{
    free(buffer);
}

static err_t spi_tx(struct netif *lwip_netif, struct pbuf *p, const ip4_addr_t *ipaddr)
{
    if (p->len >= 8)
    {
        uint8_t *b = p->payload;
        ESP_LOGD(TAG_LWIP,
                 "linkoutput([0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X%s], len=%zu)",
                 b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
                 (p->len > 8) ? " ..." : "",
                 p->len);
    }
    else
    {
        ESP_LOGD(TAG_LWIP, "linkoutput(buffer=%p, len=%zu)", p->payload, p->len);
    }

    esp_err_t err;
    if ((err = hal_spi_send(p->payload, p->len)) != ESP_OK)
    {
        ESP_LOGE(IF_NAME(lwip_netif), "hal_spi_send failed: %u", err);
    }

    return ERR_OK;
}

static void spi_rx(void *args)
{
    spi_nic_driver_t *driver = args;

    while (true)
    {
        uint8_t *buffer = malloc(SPI_MAX_PAYLOAD_SIZE);
        if (!buffer)
        {
            ESP_LOGE(TAG, "Could not allocate %u bytes for rx buffer", SPI_MAX_PAYLOAD_SIZE);
            return;
        }
        size_t recvd = SPI_MAX_PAYLOAD_SIZE;
        esp_err_t err;
        if ((err = hal_spi_recv(buffer, &recvd)) != ESP_OK)
        {
            ESP_LOGE(TAG, "hal_spi_recv failed: %u", err);
            continue;
        }

        ESP_ERROR_CHECK(esp_netif_receive(driver->spi_netif, buffer, recvd, NULL));
    }
}

static err_t cb_lwip_init(struct netif *lwip_netif)
{
    static uint8_t index = 0;

    lwip_netif->input = ip4_input;
    lwip_netif->output = spi_tx;

    lwip_netif->name[0] = 's';
    lwip_netif->name[1] = 'p';
    lwip_netif->num = index++;

    const u8_t mac[] = SPI_MAC_ADDR;
    memcpy(lwip_netif->hwaddr, mac, sizeof(lwip_netif->hwaddr));
    lwip_netif->hwaddr_len = ETH_HWADDR_LEN;
    lwip_netif->mtu = 1500;

    netif_set_flags(lwip_netif,
                    NETIF_FLAG_BROADCAST | NETIF_FLAG_LINK_UP);

    ESP_LOGI(IF_NAME(lwip_netif), "initialized");
    return ERR_OK;
}

static esp_netif_recv_ret_t cb_lwip_input(struct netif *lwip_netif, void *buffer, size_t len, void *eb)
{
    if (len >= 8)
    {
        uint8_t *b = buffer;
        ESP_LOGD(IF_NAME(lwip_netif),
                 "input([0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X%s], len=%zu)",
                 b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
                 (len > 8) ? " ..." : "",
                 len);
    }
    else
    {
        ESP_LOGD(TAG_LWIP, "input(buffer=%p, len=%zu)", buffer, len);
    }

    esp_netif_t *esp_netif = esp_netif_get_handle_from_netif_impl(lwip_netif);
    if (!esp_netif)
    {
        ESP_LOGE(TAG_LWIP, "No esp_netif handle. Has lwip_init been called before?");
        return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_FAIL);
    }

    struct pbuf *p = esp_pbuf_allocate(esp_netif, buffer, len, buffer);
    if (p == NULL)
    {
        esp_netif_free_rx_buffer(esp_netif, buffer);
        return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_ERR_NO_MEM);
    }

    if (!lwip_netif->input)
    {
        ESP_LOGE(TAG_LWIP, "No lwIP input callback. Has lwip_init been called before?");
        pbuf_free(p);
        return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_FAIL);
    }

    if (unlikely(lwip_netif->input(p, lwip_netif) != ERR_OK))
    {
        ESP_LOGE(TAG_LWIP, "IP input error");
        pbuf_free(p);
        return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_FAIL);
    }

    return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_OK);
}

static esp_err_t cb_post_attach(esp_netif_t *esp_netif, void *args)
{
    spi_nic_driver_t *driver = args;

    const esp_netif_driver_ifconfig_t driver_ifconfig = {
        .driver_free_rx_buffer = spi_netif_free_rx_buffer,
        .transmit = spi_netif_transmit,
        .handle = driver};
    driver->base.netif = esp_netif;
    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));

    if (xTaskCreate(spi_rx, "spi_rx", RX_TASK_STACK_SIZE, driver, (tskIDLE_PRIORITY + 2), NULL) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to start receive task");
        return ESP_FAIL;
    }
    
    esp_netif_action_connected(
        esp_netif_get_handle_from_ifkey(SPI_IF_KEY),
        NULL, 
        0, 
        NULL
    );
    
    return ESP_OK;
}

esp_err_t create_default_spi_netif(esp_netif_ip_info_t ip_info)
{
    ESP_ERROR_CHECK(esp_netif_init());
    spi_nic_driver_t *driver = calloc(1, sizeof(spi_nic_driver_t));

    esp_netif_inherent_config_t eth_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    eth_config.flags = ESP_NETIF_FLAG_AUTOUP;

    eth_config.ip_info = &ip_info;
    eth_config.if_key = SPI_IF_KEY;
    eth_config.route_prio = 15;

    esp_netif_config_t spi_config = ESP_NETIF_DEFAULT_ETH();
    spi_config.base = &eth_config;

    const esp_netif_netstack_config_t netstack_config = {
        .lwip = {
            .init_fn = cb_lwip_init,
            .input_fn = (input_fn_t)cb_lwip_input}};

    spi_config.stack = &netstack_config;

    driver->base.post_attach = cb_post_attach;
    driver->spi_netif = esp_netif_new(&spi_config);
    if (driver->spi_netif == NULL)
    {
        ESP_LOGE(TAG, "esp_netif_new failed!");
        free(driver);
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_netif_attach(driver->spi_netif, driver));
    esp_netif_action_start(driver->spi_netif, NULL, 0, NULL);
    return ESP_OK;
}