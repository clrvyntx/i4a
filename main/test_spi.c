#include "test_spi.h"


void test_spi_internal_message_any_sibling( void )
{
    ring_link_payload_t p = {
        .id = 1,
        .src_id = config_get_id(),
        .dst_id = CONFIG_ID_ANY,
        .buffer_type = RING_LINK_PAYLOAD_TYPE_INTERNAL,
        .buffer = "Hi neighbor!",
        .ttl = RING_LINK_PAYLOAD_TTL,
        .len = RING_LINK_PAYLOAD_BUFFER_SIZE,
    };
    printf("\n\n---------------------------\n");
    printf("SPI_INTERNAL: Testing spi_transmit to any sibling(dst_id=%i, TTL=%i)\n", \
            p.dst_id, p.ttl);
    assert(ring_link_lowlevel_transmit_payload(&p) == ESP_OK);
}

void test_spi_internal_wrong_sibling( void )
{
    ring_link_payload_t p = {
        .id = 2,
        .src_id = config_get_id(),
        .dst_id = CONFIG_ID_NONE,
        .buffer_type = RING_LINK_PAYLOAD_TYPE_INTERNAL,
        .buffer = "where is Waldo?",
        .ttl = RING_LINK_PAYLOAD_TTL,
        .len = RING_LINK_PAYLOAD_BUFFER_SIZE,
    };
    printf("\n\n---------------------------\n");
    printf("SPI_INTERNAL: Testing spi_transmit to wrong sibling(dst_id=%i, TTL=%i)\n", \
            p.dst_id, RING_LINK_PAYLOAD_TTL);
    assert(ring_link_lowlevel_transmit_payload(&p) == ESP_OK);
}

void test_spi_internal_broadcast_to_siblings( void )
{
    const char msg[] = "hello world, this is a broadcast.";
    printf("\n\n---------------------------\n");
    printf("SPI_INTERNAL: Testing broadcast_to_siblings(\"%s\", %i)\n", msg, sizeof(msg));
    assert(broadcast_to_siblings(msg, sizeof(msg)) == true);
}

void test_spi_netif_message_any_sibling( void )
{
    ring_link_payload_t p = {
        .id = 3,
        .src_id = config_get_id(),
        .dst_id = CONFIG_ID_ANY,
        .buffer_type = RING_LINK_PAYLOAD_TYPE_ESP_NETIF,
        .buffer = "this is a netif payload",
        .ttl = RING_LINK_PAYLOAD_TTL,
        .len = RING_LINK_PAYLOAD_BUFFER_SIZE,
    };
    printf("\n\n---------------------------\n");
    printf("SPI_NETIF: Testing spi_transmit to any sibling(dst_id=%i, TTL=%i)\n", \
            p.dst_id, RING_LINK_PAYLOAD_TTL);
    assert(ring_link_lowlevel_transmit_payload(&p) == ESP_OK);
}

void test_spi_netif_bouncing_message( void )
{
    ring_link_payload_t p = {
        .id = 4,
        .src_id = config_get_id(),
        .dst_id = config_get_id(),
        .buffer_type = RING_LINK_PAYLOAD_TYPE_ESP_NETIF,
        .buffer = "hi, i am back!",
        .ttl = RING_LINK_PAYLOAD_TTL,
        .len = RING_LINK_PAYLOAD_BUFFER_SIZE,
    };
    printf("\n\n---------------------------\n");
    printf("SPI_NETIF: Testing spi_transmit to myself (dst_id=%i, TTL=%i)\n", \
            p.dst_id, RING_LINK_PAYLOAD_TTL);
    assert(ring_link_lowlevel_transmit_payload(&p) == ESP_OK);
}

void test_spi_run_all(void)
{
    if (config_get_id()==1)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    test_spi_internal_message_any_sibling();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    test_spi_internal_wrong_sibling();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    test_spi_internal_broadcast_to_siblings();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    test_spi_netif_message_any_sibling();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    test_spi_netif_bouncing_message();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    printf("\n\n\n\n\n\n\n\n\n\n\n");
}