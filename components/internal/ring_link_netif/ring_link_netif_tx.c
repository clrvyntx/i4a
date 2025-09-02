#include "ring_link_netif_tx.h"

static const char* TAG = "==> ring_link_netif";

ESP_EVENT_DEFINE_BASE(RING_LINK_TX_EVENT);

static esp_netif_t *ring_link_tx_netif = NULL;
static ring_link_payload_id_t s_id_counter_tx = 0;

static const struct esp_netif_netstack_config netif_netstack_config = {
    .lwip = {
        .init_fn = ring_link_tx_netstack_init_fn,
        .input_fn = ring_link_tx_netstack_input_fn
    }
};

static const esp_netif_inherent_config_t netif_inherent_config = {
    .flags = ESP_NETIF_FLAG_AUTOUP,
    ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(mac)
    ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(ip_info)
    .get_ip_event = 0,
    .lost_ip_event = 0,
    .if_key = "ring_link_tx",
    .if_desc = "ring-link-tx if",
    .route_prio = 15,
    .bridge_info = NULL
};

static const esp_netif_config_t netif_config = {
    .base = &netif_inherent_config,
    .driver = NULL,
    .stack = &netif_netstack_config,
};


esp_netif_t *get_ring_link_tx_netif(void){
    return ring_link_tx_netif;
}

static err_t output_function(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr)
{
    // ESP_LOGI(TAG, "Calling output_function(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr)");
    // printf("len %u\n", p->len);
    // printf("total len %u\n", p->tot_len);
    ESP_LOGD(TAG, "Output function - len: %zu, tot_len: %zu", p->len, p->tot_len);
    struct ip_hdr *iphdr = (struct ip_hdr *)p->payload;
    if((IPH_V(iphdr) == 4) || (IPH_V(iphdr) == 6)){
        netif->linkoutput(netif, p);
    }else {
        ESP_LOGW(TAG, "Non-IP packet received!");
    }
    return ESP_OK;
}

static err_t linkoutput_function(struct netif *netif, struct pbuf *p)
{
    // ESP_LOGI(TAG, "Calling linkoutput_function(struct netif *netif, struct pbuf *p)");

    struct pbuf *q = p;
    u16_t alloc_len = (u16_t)(p->tot_len);
    // ip4_debug_print(q);
    esp_netif_t *esp_netif = esp_netif_get_handle_from_netif_impl(netif);
    esp_err_t ret = ESP_FAIL;

    if (!esp_netif) {
        LWIP_DEBUGF(NETIF_DEBUG, ("corresponding esp-netif is NULL: netif=%p pbuf=%p len=%d\n", netif, p, p->len));
        return ERR_IF;
    }
    if (q->next == NULL) {
        ret = esp_netif_transmit(esp_netif, q->payload, q->len);
    } else {
        ESP_LOGE(TAG, "low_level_output: pbuf is a list, application may has bug");
        q = pbuf_alloc(PBUF_RAW_TX, p->tot_len, PBUF_RAM);
        if (q != NULL) {
            pbuf_copy(q, p);
        } else {
            return ERR_MEM;
        }
        ret = esp_netif_transmit(esp_netif, q->payload, q->len);
        /* content in payload has been copied to DMA buffer, it's safe to free pbuf now */
        pbuf_free(q);
    }
    /* Check error */
    if (likely(ret == ESP_OK)) {
        return ERR_OK;
    }
    if (ret == ESP_ERR_NO_MEM) {
        return ERR_MEM;
    }
    return ERR_IF;

}

static esp_err_t esp_netif_ring_link_driver_transmit(void *h, void *buffer, size_t len)
{
    // ESP_LOGI(TAG, "ring_link_netif_driver_transmit(void *h, void *buffer, size_t len) called");
    static uint32_t packet_count = 0;
    packet_count++;
    ESP_LOGD(TAG, "[%" PRIu32 "] TX Packet - len: %zu", packet_count, len);
    if (buffer==NULL) {
        ESP_LOGI(TAG, "buffer is null");
        return ESP_OK;
    }
    ring_link_payload_t p = {
        .id = s_id_counter_tx ++,
        .ttl = RING_LINK_PAYLOAD_TTL,
        .src_id = config_get_id(),
        .dst_id = CONFIG_ID_ANY,
        .buffer_type = RING_LINK_PAYLOAD_TYPE_ESP_NETIF,
        .len = len,
    };
    if (len > RING_LINK_PAYLOAD_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Buffer length exceeds maximum allowed size.");
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(p.buffer, buffer, len);
    ESP_LOGD(TAG, "[%" PRIu32 "] Transmitting payload - id: %d, src: %d, dst: %d", 
             packet_count, p.id, p.src_id, p.dst_id);
    return ring_link_lowlevel_transmit_payload(&p);
}

err_t ring_link_tx_netstack_init_fn(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    netif->name[0]= 's';
    netif->name[1] = 'p';
    netif->output = output_function;
    netif->linkoutput = linkoutput_function;
    netif->mtu = RING_LINK_NETIF_MTU;
    netif->hwaddr_len = ETH_HWADDR_LEN;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_LINK_UP;
    return ERR_OK;
}

esp_netif_recv_ret_t ring_link_tx_netstack_input_fn(void *h, void *buffer, size_t len, void* l2_buff)
{
    return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_OK);
}

static esp_err_t esp_netif_ring_link_driver_transmit_wrap(void *h, void *buffer, size_t len, void *netstack_buffer)
{
    printf("transmit_wrap_function\n");
    return ESP_OK;
};

static esp_err_t ring_link_tx_driver_post_attach(esp_netif_t * esp_netif, void * args)
{
    ESP_LOGI(TAG, "Calling esp_netif_ring_link_post_attach_start(esp_netif_t * esp_netif, void *args)");
    ring_link_netif_driver_t driver = (ring_link_netif_driver_t) args;
    driver->base.netif = esp_netif;
    esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle = driver,
        .transmit = esp_netif_ring_link_driver_transmit,
        .transmit_wrap = esp_netif_ring_link_driver_transmit_wrap,
        .driver_free_rx_buffer = NULL,
    };

    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));

    return ESP_OK;
}

static void ring_link_tx_default_handler(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    ESP_LOGI(TAG, "Calling ring_link_tx_default_handler");
    esp_netif_action_got_ip(ring_link_tx_netif, base, event_id, data);
}


static void ring_link_tx_default_action_start(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    ESP_LOGI(TAG, "Calling ring_link_tx_default_action_start");
    const esp_netif_ip_info_t ip_info = config_get_tx_ip_info();

    ESP_ERROR_CHECK(esp_netif_set_ip_info(ring_link_tx_netif, &ip_info));

    esp_netif_action_start(ring_link_tx_netif, base, event_id, data);
    ESP_ERROR_CHECK(esp_netif_set_default_netif(ring_link_tx_netif));
}

esp_err_t ring_link_tx_netif_init(void)
{
    ESP_LOGI(TAG, "Calling ring_link_tx_netif_init");

    ring_link_tx_netif = ring_link_netif_new(&netif_config);
    
    ESP_ERROR_CHECK(ring_link_netif_esp_netif_attach(ring_link_tx_netif, ring_link_tx_driver_post_attach));

    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error getting MAC address");
        return ret;
    }
    esp_netif_set_mac(ring_link_tx_netif, mac);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(RING_LINK_TX_EVENT, RING_LINK_EVENT_START, ring_link_tx_default_action_start, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_post(RING_LINK_TX_EVENT, RING_LINK_EVENT_START, NULL, 0, portMAX_DELAY));
    return ESP_OK;
}

esp_ip4_addr_t get_spi_tx_ip_interface_address(void){
    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(ring_link_tx_netif, &ip_info));
    return ip_info.ip;
}