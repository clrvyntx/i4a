#include "ring_link_netif_rx.h"

static const char* TAG = "==> ring_link_netif_rx";

ESP_EVENT_DEFINE_BASE(RING_LINK_RX_EVENT);

static esp_netif_t *ring_link_rx_netif = NULL;
static ring_link_payload_id_t s_id_counter_rx = 0;

/* ------------------ Static Buffer Pool ------------------ */
#define RX_PBUF_NUM 20

static struct pbuf rx_pbufs[RX_PBUF_NUM];
static uint8_t rx_data[RX_PBUF_NUM][RING_LINK_NETIF_MTU];
static uint8_t rx_buffer_used[RX_PBUF_NUM];
static portMUX_TYPE s_rx_lock = portMUX_INITIALIZER_UNLOCKED;

/* ------------------ Netif Config ------------------ */
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

/* ------------------ RX Function ------------------ */
esp_err_t ring_link_rx_netif_receive(ring_link_payload_t *p)
{
    struct pbuf *q;
    struct ip_hdr *ip_header;
    esp_err_t error;

    if (p->len <= 0 || p->len < IP_HLEN) {
        ESP_LOGW(TAG, "Discarding invalid payload.");
        return ESP_OK;
    }

    if (p->len > RING_LINK_NETIF_MTU) {
        ESP_LOGW(TAG, "Payload exceeds MTU (%d > %d). Dropping packet.", p->len, RING_LINK_NETIF_MTU);
        return ESP_ERR_INVALID_SIZE;
    }

    ip_header = (struct ip_hdr *)p->buffer;

    if (!((IPH_V(ip_header) == 4) || (IPH_V(ip_header) == 6))) {
        ESP_LOGW(TAG, "Discarding non-IP payload.");
        return ESP_OK;
    }

    u16_t iphdr_len = lwip_ntohs(IPH_LEN(ip_header));

    if (iphdr_len > p->len) {
        ESP_LOGW(TAG, "Payload size (%d) smaller than IP length (%d). Dropping packet.", p->len, iphdr_len);
        return ESP_OK;
    }

    /* --------- Find free static buffer --------- */
    int i;
    portENTER_CRITICAL(&s_rx_lock);
    for (i = 0; i < RX_PBUF_NUM; i++) {
        if (!rx_buffer_used[i]) break;
    }
    if (i == RX_PBUF_NUM) {
        portEXIT_CRITICAL(&s_rx_lock);
        ESP_LOGW(TAG, "No free RX buffer. Dropping packet.");
        return ESP_ERR_NO_MEM;
    }
    rx_buffer_used[i] = 1;

    /* Copy packet into static buffer inside critical section */
    memcpy(rx_data[i], p->buffer, iphdr_len);

    /* Setup pbuf */
    rx_pbufs[i].payload = rx_data[i];
    rx_pbufs[i].len = iphdr_len;
    rx_pbufs[i].tot_len = iphdr_len;
    rx_pbufs[i].next = NULL;
    q = &rx_pbufs[i];
    portEXIT_CRITICAL(&s_rx_lock);

    /* Pass pbuf to netif */
    error = esp_netif_receive(ring_link_rx_netif, q, q->tot_len, NULL);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "esp_netif_receive failed");
        /* Free buffer safely */
        portENTER_CRITICAL(&s_rx_lock);
        rx_buffer_used[i] = 0;
        portEXIT_CRITICAL(&s_rx_lock);
    }

    return error;
}

/* ------------------ Free Static Pbuf ------------------ */
void rx_pbuf_free(struct pbuf *p)
{
    portENTER_CRITICAL(&s_rx_lock);
    for (int i = 0; i < RX_PBUF_NUM; i++) {
        if (&rx_pbufs[i] == p) {
            rx_buffer_used[i] = 0;
            break;
        }
    }
    portEXIT_CRITICAL(&s_rx_lock);
}

/* ------------------ LWIP Output Functions ------------------ */
static err_t output_function(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr)
{
    struct ip_hdr *iphdr = (struct ip_hdr *)p->payload;
    if((IPH_V(iphdr) == 4) || (IPH_V(iphdr) == 6)){
        netif->linkoutput(netif, p);
    }
    return ESP_OK;
}

static err_t linkoutput_function(struct netif *netif, struct pbuf *p)
{
    struct pbuf *q = p;
    esp_netif_t *esp_netif = esp_netif_get_handle_from_netif_impl(netif);
    esp_err_t ret = ESP_FAIL;

    if (!esp_netif) return ERR_IF;

    if (q->next == NULL) {
        ret = esp_netif_transmit(esp_netif, q->payload, q->len);
    } else {
        ESP_LOGE(TAG, "pbuf list not supported. Copying.");
        q = pbuf_alloc(PBUF_RAW_TX, p->tot_len, PBUF_RAM);
        if (!q) return ERR_MEM;
        pbuf_copy(q, p);
        ret = esp_netif_transmit(esp_netif, q->payload, q->len);
        pbuf_free(q);
    }

    if (likely(ret == ESP_OK)) return ERR_OK;
    if (ret == ESP_ERR_NO_MEM) return ERR_MEM;
    return ERR_IF;
}

/* ------------------ LWIP Netif Init ------------------ */
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

/* ------------------ LWIP Netif Input ------------------ */
esp_netif_recv_ret_t ring_link_rx_netstack_lwip_input_fn(void *h, void *buffer, size_t len, void *l2_buff)
{
    struct netif *netif = h;
    err_t result;

    if (unlikely(!buffer || !netif_is_up(netif))) {
        if (l2_buff) esp_netif_free_rx_buffer(netif->state, l2_buff);
        return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_FAIL);
    }

    struct pbuf *p = (struct pbuf *)buffer;
    result = netif->input(p, netif);

    if (unlikely(result != ERR_OK)) {
        ESP_LOGE(TAG, "netif->input error: %d", result);
        return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_FAIL);
    }

    return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_OK);
}

/* ------------------ Driver Transmit ------------------ */
static esp_err_t ring_link_rx_driver_transmit(void *h, void *buffer, size_t len)
{
    ESP_LOGW(TAG, "This netif should not transmit");
    return ESP_OK;
}

static esp_err_t esp_netif_ring_link_driver_transmit(void *h, void *buffer, size_t len)
{
    if (!buffer) return ESP_OK;

    ring_link_payload_t p = {
        .id = s_id_counter_rx++,
        .ttl = RING_LINK_PAYLOAD_TTL,
        .src_id = config_get_id(),
        .dst_id = CONFIG_ID_ANY,
        .buffer_type = RING_LINK_PAYLOAD_TYPE_ESP_NETIF,
        .len = len,
    };

    if (len > RING_LINK_PAYLOAD_BUFFER_SIZE) return ESP_ERR_INVALID_SIZE;

    memcpy(p.buffer, buffer, len);
    return ring_link_lowlevel_transmit_payload(&p);
}

/* ------------------ Post Attach ------------------ */
static esp_err_t ring_link_rx_driver_post_attach(esp_netif_t * esp_netif, void * args)
{
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

/* ------------------ Event Handlers ------------------ */
static void ring_link_rx_default_action_start(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    const esp_netif_ip_info_t ip_info = config_get_rx_ip_info();
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ring_link_rx_netif, &ip_info));

    uint8_t mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    esp_netif_set_mac(ring_link_rx_netif, mac);

    esp_netif_action_start(ring_link_rx_netif, base, event_id, data);
}

/* ------------------ Netif Init ------------------ */
esp_err_t ring_link_rx_netif_init(void)
{
    ESP_LOGI(TAG, "Initializing RX netif");
    ring_link_rx_netif = ring_link_netif_new(&netif_config);

    ESP_ERROR_CHECK(ring_link_netif_esp_netif_attach(ring_link_rx_netif, ring_link_rx_driver_post_attach));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(RING_LINK_RX_EVENT, RING_LINK_EVENT_START, ring_link_rx_default_action_start, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_post(RING_LINK_RX_EVENT, RING_LINK_EVENT_START, NULL, 0, portMAX_DELAY));

    return ESP_OK;
}
