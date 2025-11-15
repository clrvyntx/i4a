#include "ring_link_netif_rx.h"

static const char* TAG = "==> ring_link_netif_rx";

ESP_EVENT_DEFINE_BASE(RING_LINK_RX_EVENT);

static esp_netif_t *ring_link_rx_netif = NULL;
static ring_link_payload_id_t s_id_counter_rx = 0;

static const struct esp_netif_netstack_config netif_netstack_config = {
    .lwip = {
        .init_fn = ring_link_rx_netstack_lwip_init_fn,
        .input_fn = ring_link_rx_netstack_lwip_input_fn
    }
};

static const esp_netif_inherent_config_t netif_inherent_config = {
    .flags = ESP_NETIF_FLAG_AUTOUP,
    ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(mac)
    ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(ip_info)
    .get_ip_event = 0,
    .lost_ip_event = 0,
    .if_key = "ring_link_rx",
    .if_desc = "ring-link-rx if",
    .route_prio = 15,
    .bridge_info = NULL
};

static const esp_netif_config_t netif_config = {
    .base = &netif_inherent_config,
    .driver = NULL,
    .stack = &netif_netstack_config,
};

esp_err_t ring_link_rx_netif_receive(ring_link_payload_t *p)
{
    struct pbuf *q;
    struct ip_hdr *ip_header;
    esp_err_t error;

    if (p->len <= 0 || p->len < IP_HLEN) {
        ESP_LOGW(TAG, "Discarding invalid payload.");
        return ESP_OK;
    }

    ip_header = (struct ip_hdr *)p->buffer;

    if (!((IPH_V(ip_header) == 4) || (IPH_V(ip_header) == 6))) {
        ESP_LOGW(TAG, "Discarding non-IP payload.");
        return ESP_OK;
    }

    // ip4_addr_t src_ip;
    // src_ip.addr = ip_header->src.addr;

    // // Convert IP address to host format (little-endian if needed)
    // uint32_t addr_ = ntohl(src_ip.addr); // Convert address to host format

    // // Extract octets
    // uint8_t octet1_ = (addr_ >> 24) & 0xFF; // First octet
    // uint8_t octet2_ = (addr_ >> 16) & 0xFF; // Second octet

    // // Print IP address for debugging
    // ESP_LOGD(TAG, "SRC IP: %d.%d.%d.%d", 
    //     octet1_, octet2_, (int)((addr_ >> 8) & 0xFF), (int)(addr_ & 0xFF));

    // // Print destination IP address
    // ip4_addr_t dest_ip;
    // dest_ip.addr = ip_header->dest.addr;

    // // Convert IP address to host format (little-endian if needed)
    // uint32_t addr = ntohl(dest_ip.addr); // Convert address to host format

    // // Extract octets
    // uint8_t octet1 = (addr >> 24) & 0xFF; // First octet
    // uint8_t octet2 = (addr >> 16) & 0xFF; // Second octet

    // // Print IP address for debugging
    // ESP_LOGD(TAG, "Destination IP: %d.%d.%d.%d", 
    //     octet1, octet2, (int)((addr >> 8) & 0xFF), (int)(addr & 0xFF));

    // Filter by subnet 192.170.x.x
    // if (octet1 != 192 || octet2 != 170) {
    //     ESP_LOGW(TAG, "Discarding packet not in 192.170.x.x subnet.");
    //     return ESP_OK;
    // }

    // Total IP packet size from header
    u16_t iphdr_len = lwip_ntohs(IPH_LEN(ip_header));

    // Validate that payload size is sufficient
    if (iphdr_len > p->len) {
        ESP_LOGW(TAG, "Payload size (%d) is smaller than IP length (%d). Discarding packet.", p->len, iphdr_len);
        return ESP_OK;
    }

    // Allocate pbuf with required size
    q = pbuf_alloc(PBUF_TRANSPORT, iphdr_len, PBUF_POOL);
    if (q == NULL) {
        ESP_LOGW(TAG, "Failed to allocate pbuf.");
        return ESP_FAIL;
    }

    // Copy data to pbuf
    memcpy(q->payload, p->buffer, iphdr_len);
    q->next = NULL;
    q->len = iphdr_len;
    q->tot_len = iphdr_len;
    
    //ip4_debug_print(q);

    // Pass pbuf to esp_netif
    error = esp_netif_receive(ring_link_rx_netif, q, q->tot_len, NULL);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "process_thread_receive failed.");
        pbuf_free(q);
    }

    return error;
    }

static err_t output_function(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr)
{
    // ESP_LOGI(TAG, "Calling output_function(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr)");
    // printf("len %u\n", p->len);
    // printf("total len %u\n", p->tot_len);
    struct ip_hdr *iphdr = (struct ip_hdr *)p->payload;
    if((IPH_V(iphdr) == 4) || (IPH_V(iphdr) == 6)){
        netif->linkoutput(netif, p);
    }
    return ESP_OK;
}

static err_t linkoutput_function(struct netif *netif, struct pbuf *p)
{
    // ESP_LOGI(TAG, "Calling linkoutput_function(struct netif *netif, struct pbuf *p)");

    struct pbuf *q = p;
    u16_t alloc_len = (u16_t)(p->tot_len);
    ip4_debug_print(q);
    esp_netif_t *esp_netif = esp_netif_get_handle_from_netif_impl(netif);
    esp_err_t ret = ESP_FAIL;

    if (!esp_netif) {
        LWIP_DEBUGF(NETIF_DEBUG, ("corresponding esp-netif is NULL: netif=%p pbuf=%p len=%d\n", netif, p, p->len));
        return ERR_IF;
    }
    if (q->next == NULL) {
        ret = esp_netif_transmit(esp_netif, q->payload, q->len);
    } else {
        // LWIP_DEBUGF(PBUF_DEBUG, ("low_level_output: pbuf is a list, application may has bug"));
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

err_t ring_link_rx_netstack_lwip_init_fn(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    netif->name[0]= 'r';
    netif->name[1] = 'x';
    netif->output = output_function;
    netif->linkoutput = linkoutput_function;
    netif->mtu = RING_LINK_NETIF_MTU;
    netif->hwaddr_len = ETH_HWADDR_LEN;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_LINK_UP;
    return ERR_OK;
}

esp_netif_recv_ret_t ring_link_rx_netstack_lwip_input_fn(void *h, void *buffer, size_t len, void *l2_buff)
{
    struct netif *netif = h;
    err_t result;  // Variable para almacenar el código de retorno

    /* Verifica que el buffer y la interfaz sean válidos */
    if (unlikely(!buffer || !netif_is_up(netif))) {
        if (l2_buff) {
            esp_netif_free_rx_buffer(netif->state, l2_buff);
        }
        return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_FAIL);
    }

    /* Usa directamente el buffer recibido */
    struct pbuf *p = (struct pbuf *)buffer;

    /* Llama a la función de entrada y captura el resultado */
    result = netif->input(p, netif);

    /* Imprime el código de error si hay problemas */
    if (unlikely(result != ERR_OK)) {
        ESP_LOGE("ring_link", "netif->input error: %d", result);
        return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_FAIL);
    }

    /* Todo salió bien */
    return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_OK);
}

static esp_err_t ring_link_rx_driver_transmit(void *h, void *buffer, size_t len)
{
    ESP_LOGW(TAG, "This netif should not transmit but ring_link_rx_driver_transmit was called");
    return ESP_OK;
}

static esp_err_t esp_netif_ring_link_driver_transmit(void *h, void *buffer, size_t len)
{
    // ESP_LOGI(TAG, "ring_link_netif_driver_transmit(void *h, void *buffer, size_t len) called");
    
    if (buffer==NULL) {
        ESP_LOGI(TAG, "buffer is null");
        return ESP_OK;
    }
    ring_link_payload_t p = {
        .id = s_id_counter_rx++,
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
    return ring_link_lowlevel_transmit_payload(&p);
}

static esp_err_t ring_link_rx_driver_post_attach(esp_netif_t * esp_netif, void * args)
{
    ESP_LOGI(TAG, "Calling esp_netif_ring_link_post_attach_start(esp_netif_t * esp_netif, void *args)");
    ring_link_netif_driver_t driver = (ring_link_netif_driver_t) args;
    driver->base.netif = esp_netif;
    esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle = driver,
        .transmit = esp_netif_ring_link_driver_transmit,
        .driver_free_rx_buffer = NULL,
    };

    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));
    return ESP_OK;
}

static void ring_link_rx_default_handler(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    ESP_LOGI(TAG, "Calling ring_link_rx_default_handler");
    esp_netif_action_got_ip(ring_link_rx_netif, base, event_id, data);
}

static void ring_link_rx_default_action_start(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    ESP_LOGI(TAG, "Calling ring_link_rx_default_action_start");

    const esp_netif_ip_info_t ip_info = config_get_rx_ip_info();
    
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ring_link_rx_netif, &ip_info));

    uint8_t mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    esp_netif_set_mac(ring_link_rx_netif, mac);
    esp_netif_action_start(ring_link_rx_netif, base, event_id, data);
}


esp_err_t ring_link_rx_netif_init(void)
{
    ESP_LOGI(TAG, "Calling ring_link_rx_netif_init");
    ring_link_rx_netif = ring_link_netif_new(&netif_config);

    ESP_ERROR_CHECK(ring_link_netif_esp_netif_attach(ring_link_rx_netif, ring_link_rx_driver_post_attach));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(RING_LINK_RX_EVENT, RING_LINK_EVENT_START, ring_link_rx_default_action_start, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_post(RING_LINK_RX_EVENT, RING_LINK_EVENT_START, NULL, 0, portMAX_DELAY));
    return ESP_OK;
}
