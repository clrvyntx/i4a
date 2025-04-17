#include "ring_link.h"
#include "unity.h"
#include <stdbool.h>

static bool internal_handler_called = false;
static bool netif_handler_called = false;

static esp_err_t mock_internal_handler(ring_link_payload_t *p) {
    internal_handler_called = true;
    return ESP_OK;
}

static esp_err_t mock_netif_handler(ring_link_payload_t *p) {
    netif_handler_called = true;
    return ESP_OK;
}

static ring_link_handlers_t original_handlers;

void ring_link_test_setup(void) {
    original_handlers = ring_link_handlers;
    ring_link_handlers.internal_handler = mock_internal_handler;
    ring_link_handlers.netif_handler = mock_netif_handler;
    internal_handler_called = false;
    netif_handler_called = false;
}

void ring_link_test_teardown(void) {
    ring_link_handlers = original_handlers;
}

TEST_CASE("Test process_ring_link_payload with INTERNAL type", "[ring_link]")
{
    ring_link_test_setup();
    ring_link_payload_t p;
    p.buffer_type = RING_LINK_PAYLOAD_TYPE_INTERNAL;
    TEST_ASSERT_EQUAL(ESP_OK, process_ring_link_payload(&p));
    TEST_ASSERT_TRUE(internal_handler_called);
    TEST_ASSERT_FALSE(netif_handler_called);
    ring_link_test_teardown();
}

TEST_CASE("Test process_ring_link_payload with INTERNAL_HEARTBEAT type", "[ring_link]")
{
    ring_link_test_setup();
    ring_link_payload_t p;
    p.buffer_type = RING_LINK_PAYLOAD_TYPE_INTERNAL_HEARTBEAT;
    TEST_ASSERT_EQUAL(ESP_OK, process_ring_link_payload(&p));
    TEST_ASSERT_TRUE(internal_handler_called);
    TEST_ASSERT_FALSE(netif_handler_called);
    ring_link_test_teardown();
}

TEST_CASE("Test process_ring_link_payload with ESP_NETIF type", "[ring_link]")
{
    ring_link_test_setup();
    ring_link_payload_t p;
    p.buffer_type = RING_LINK_PAYLOAD_TYPE_ESP_NETIF;
    TEST_ASSERT_EQUAL(ESP_OK, process_ring_link_payload(&p));
    TEST_ASSERT_FALSE(internal_handler_called);
    TEST_ASSERT_TRUE(netif_handler_called);
    ring_link_test_teardown();
}

TEST_CASE("Test process_ring_link_payload with unknown type", "[ring_link]")
{
    ring_link_test_setup();
    ring_link_payload_t p;
    p.buffer_type = 99;  // Unknown type
    TEST_ASSERT_EQUAL(ESP_FAIL, process_ring_link_payload(&p));
    TEST_ASSERT_FALSE(internal_handler_called);
    TEST_ASSERT_FALSE(netif_handler_called);
    ring_link_test_teardown();
}