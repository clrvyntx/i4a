#include "ring_link_payload.h"
#include "esp_crc.h"

static const char* TAG = "==> ring_link_payload";


bool ring_link_payload_is_for_device(ring_link_payload_t *p)
{
    return p->dst_id == config_get_id() \
        || (p->dst_id == CONFIG_ID_ANY);
}

bool ring_link_payload_is_from_device(ring_link_payload_t *p)
{
    return p->src_id == config_get_id();
}

bool ring_link_payload_is_broadcast(ring_link_payload_t *p)
{
    return p->dst_id == CONFIG_ID_ALL;
}

bool ring_link_payload_is_internal(ring_link_payload_t *p)
{
    return p->buffer_type == RING_LINK_PAYLOAD_TYPE_INTERNAL;
}

bool ring_link_payload_is_esp_netif(ring_link_payload_t *p)
{
    return p->buffer_type == RING_LINK_PAYLOAD_TYPE_ESP_NETIF;
}

uint32_t ring_link_compute_crc32(const ring_link_payload_t *p) {
    return esp_crc32_le(0, (const uint8_t *)p, RING_LINK_PAYLOAD_CRC_LEN);
}
