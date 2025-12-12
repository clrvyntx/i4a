#include "lwip/esp_netif_net_stack.h"
#include "lwip/esp_pbuf_ref.h"
#include "netif/etharp.h"
#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "virtual_nic.h"

#define TPP_TASK_STACK_SIZE 4096

#define IF_NAME(x) esp_netif_get_ifkey(esp_netif_get_handle_from_netif_impl(x))
#define TAG "vnic"
#define TAG_LWIP TAG "_lwip"
#define TAG_ESPN TAG "_esp"

#define VNIC_MAC_ADDR                      \
    {                                      \
        0xaa, 0x12, 0x34, 0x56, 0x78, 0x9a \
    }

typedef enum
{
    VNIC_EVENT_START,
    VNIC_EVENT_STOP,

} vnic_event_t;

ESP_EVENT_DEFINE_BASE(VNIC_EVENT);

static err_t cb_lwip_init(struct netif *inp);
static esp_netif_recv_ret_t cb_lwip_input(struct netif *lwip_netif, void *buffer, size_t len, void *eb);
static err_t cb_lwip_linkoutput(struct netif *netif, struct pbuf *p);

typedef struct vnic_driver
{
    esp_netif_driver_base_t base; /*!< base structure reserved as esp-netif driver */

    // Custom fields
    vnic_t *vnic;
    esp_netif_t *vnic_netif;
} vnic_driver_t;

static err_t cb_lwip_output(struct netif *lwip_netif, struct pbuf *p, const ip4_addr_t *ipaddr) {
    // There seems to be a bug in lwIP where you cannot forward a packet from an L3/IP-only 
    // nic to an L2/Ethernet nic because the `etharp_output` function does not have enough space
    // to add L2 headers. This just re-allocs a new pbuf with more space for the headers.
    struct pbuf *pb = pbuf_alloc(PBUF_LINK, p->tot_len, PBUF_POOL);
    pbuf_copy(pb, p);
    err_t ret = etharp_output(lwip_netif, pb, ipaddr);
    pbuf_free(pb);
    return ret;
}

static esp_err_t cb_vnic_transmit(void *h, void *buffer, size_t len)
{
    // TODO: I'm not sure what's this callback for
    ESP_LOGE(TAG, "Unimplemented: cb_vnic_transmit(h=%p, buffer=%p, len=%zu)", h, buffer, len);
    return ESP_OK;
}

static void cb_vnic_free_rx_buffer(void *h, void *buffer)
{
    free(buffer);
}

static void th_vnic_rx(void *h)
{
    vnic_driver_t *driver = h;
    vnic_t *nic = driver->vnic;

    while (true)
    {
        uint8_t *buffer = malloc(VNIC_MAX_LEN);
        if (!buffer)
        {
            ESP_LOGE(TAG, "Could not allocate %u bytes for rx buffer", VNIC_MAX_LEN);
            return;
        }
        size_t recvd = 0;
        vnic_result_t err;
        if ((err = vnic_receive(nic, buffer, VNIC_MAX_LEN, &recvd)) != VNIC_OK)
        {
            ESP_LOGE(TAG, "vnic_receive failed: %u", err);
            continue;
        }

        ESP_ERROR_CHECK(esp_netif_receive(driver->vnic_netif, buffer, recvd, NULL));
    }
}

static void vnic_driver_start(void *h)
{
    if (xTaskCreate(th_vnic_rx, "th_vnic_rx", TPP_TASK_STACK_SIZE, h, (tskIDLE_PRIORITY + 2), NULL) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to start receive task");
    }
}

static esp_err_t cb_post_attach(esp_netif_t *esp_netif, void *args)
{
    vnic_driver_t *driver = args;

    const esp_netif_driver_ifconfig_t driver_ifconfig = {
        .driver_free_rx_buffer = cb_vnic_free_rx_buffer,
        .transmit = cb_vnic_transmit,
        .handle = driver};
    driver->base.netif = esp_netif;
    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));
    return ESP_OK;
}

vnic_result_t vnic_register_esp_netif(vnic_t *self, esp_netif_config_t config)
{
    ESP_ERROR_CHECK(esp_netif_init());
    vnic_driver_t *driver = calloc(1, sizeof(vnic_driver_t));
    driver->vnic = self;
    self->esp_driver = driver;
    if (!self->esp_driver)
    {
        return VNIC_NO_MEMORY;
    }

    const esp_netif_netstack_config_t netstack_config = {
        .lwip = {
            .init_fn = cb_lwip_init,
            .input_fn = (input_fn_t)cb_lwip_input}};

    config.stack = &netstack_config;

    driver->base.post_attach = cb_post_attach;
    driver->vnic_netif = esp_netif_new(&config);
    if (driver->vnic_netif == NULL)
    {
        ESP_LOGE(TAG, "esp_netif_new failed!");
        free(driver);
        self->esp_driver = NULL;
        return VNIC_INVALID_PARAM;
    }

    ESP_ERROR_CHECK(esp_netif_attach(driver->vnic_netif, driver));
    esp_netif_action_start(driver->vnic_netif, VNIC_EVENT, VNIC_EVENT_START, NULL);
    vnic_driver_start(driver);
    return VNIC_OK;
}

/********************* lwIP callbacks *********************/

/// Callback for lwIP Initialization
///
/// esp_netif_action_start triggers this callback
///
/// Sets up the internal lwIP netif structure callbacks.
static err_t cb_lwip_init(struct netif *lwip_netif)
{
    static uint8_t index = 0;

    lwip_netif->input = ethernet_input;
    lwip_netif->output = cb_lwip_output;
    lwip_netif->linkoutput = cb_lwip_linkoutput;

    lwip_netif->name[0] = 'v';
    lwip_netif->name[1] = 'n';
    lwip_netif->num = index++;

    const u8_t mac[] = VNIC_MAC_ADDR;
    memcpy(lwip_netif->hwaddr, mac, sizeof(lwip_netif->hwaddr));
    lwip_netif->hwaddr_len = ETH_HWADDR_LEN;
    lwip_netif->mtu = 1500;

    netif_set_flags(lwip_netif,
                    NETIF_FLAG_BROADCAST | NETIF_FLAG_LINK_UP | NETIF_FLAG_ETHARP);

    ESP_LOGI(IF_NAME(lwip_netif), "initialized");
    return ERR_OK;
}

/// Callback for lwIP packet input
///
/// esp_netif_receive triggers this callback
///
/// Called when a new packet has been received from the interface.
/// It should send it to the callback provided by lwIP at netif->input.
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

static err_t cb_lwip_linkoutput(struct netif *lwip_netif, struct pbuf *p)
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

    esp_netif_t *esp_netif = esp_netif_get_handle_from_netif_impl(lwip_netif);
    vnic_driver_t *driver = esp_netif_get_io_driver(esp_netif);
    vnic_result_t err;
    if ((err = vnic_transmit(driver->vnic, p->payload, p->len)) != VNIC_OK)
    {
        ESP_LOGE(IF_NAME(lwip_netif), "vnic_transmit failed: %u", err);
    }

    return ERR_OK;
}