#include <stdio.h>
#include <string.h>
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "unity.h"
#include "utils.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "memory_checks.h"
#include "esp_heap_caps.h"

#include "lwip_custom_hooks.h"
#include "route.h"
#include "ring_link.h"
#include "wifi.h"


#define TAG "route test->"

struct netif *test_netif_list;

#undef CUSTOM_NETIF_LIST
#define CUSTOM_NETIF_LIST test_netif_list

bool init_tests = false;


void print_memory_info() {
    printf("Free heap: %ld\n", esp_get_free_heap_size());
    printf("Minimum free heap ever: %ld\n", esp_get_minimum_free_heap_size());
    
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
    printf("Total free bytes: %d\n", info.total_free_bytes);
    printf("Total allocated bytes: %d\n", info.total_allocated_bytes);
    printf("Largest free block: %d\n", info.largest_free_block);
}

esp_err_t init_nvs(void){

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "nvs initialized");
    return ESP_OK;
}

void setup_custom_hook_test()
{
    // La inicializacion solo corre una vez
    if(!init_tests){
        ESP_ERROR_CHECK(init_nvs());
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        config_setup();
        config_print();
        ESP_ERROR_CHECK(ring_link_init());
        ESP_ERROR_CHECK(wifi_init());
        ESP_ERROR_CHECK(wifi_netif_init());
        init_tests = true;
    }
    test_utils_record_free_mem();
    TEST_ESP_OK(test_utils_set_leak_level(0, ESP_LEAK_TYPE_CRITICAL, ESP_COMP_LEAK_GENERAL));

}

void teardown_custom_hook_test()
{
    test_utils_finish_and_evaluate_leaks(test_utils_get_leak_level(ESP_LEAK_TYPE_WARNING, ESP_COMP_LEAK_ALL),
                                         test_utils_get_leak_level(ESP_LEAK_TYPE_CRITICAL, ESP_COMP_LEAK_ALL));
}

TEST_CASE("Test: add static route", "[route]")
{   
    setup_custom_hook_test();

    unsigned initial_routes = get_num_routes();

    ESP_ERROR_CHECK(add_route("192.168.1.1", SPI_GATEWAY, "255.255.255.0", "a"));

    TEST_ASSERT_EQUAL(initial_routes + 1, get_num_routes());
    ESP_ERROR_CHECK(rm_route("192.168.1.1", "255.255.255.0"));

    TEST_ASSERT_EQUAL(initial_routes, get_num_routes());

    teardown_custom_hook_test();
}


TEST_CASE("Test: route to SPI network", "[route]")
{   
    setup_custom_hook_test();

    // Se chequea el numero inicial de rutas
    unsigned initial_routes = get_num_routes();
    printf("Número inicial de rutas: %d\n", initial_routes);
    TEST_ASSERT_EQUAL(initial_routes, 1);

    // Se agregan las rutas
    ESP_ERROR_CHECK(add_route("192.168.1.1", SPI_GATEWAY, "255.255.255.0", "a"));
    ESP_ERROR_CHECK(add_route("192.168.1.100", WIFI_GATEWAY, "255.255.0.0", "b"));
    ESP_ERROR_CHECK(add_route("10.10.10.150", WIFI_GATEWAY, "255.255.255.0", "c"));
    
    // Retrive the right interface -> SPI
    ip4_addr_t src_addr;
    ip4_addr_t dest_addr;

    // Convertir las cadenas de dirección IP a estructuras ip4_addr_t
    IP4_ADDR(&src_addr, 10, 0, 0, 1);  // 10.0.0.1
    IP4_ADDR(&dest_addr, 192, 168, 1, 25);  // 192.168.1.25

    // Llamar a la función con las estructuras ip4_addr_t
    struct netif * netif_route = custom_ip4_route_src_hook(&src_addr, &dest_addr);
    TEST_ASSERT_NOT_NULL(netif_route);
    
    // Check tx interface
    esp_netif_t * tx_netif = get_ring_link_tx_netif();
    struct netif * tx_netif_impl = esp_netif_get_netif_impl(tx_netif);
    TEST_ASSERT_EQUAL(netif_route, tx_netif_impl);

    // Check number of routes
    TEST_ASSERT_EQUAL(initial_routes + 3, get_num_routes());
    
    // Se eliminan las rutas
    ESP_ERROR_CHECK(rm_route("192.168.1.1", "255.255.255.0")); //a
    ESP_ERROR_CHECK(rm_route("192.168.1.100", "255.255.0.0")); //b
    ESP_ERROR_CHECK(rm_route("10.10.10.150", "255.255.255.0")); //c

    // Se chequea que se hayan eliminado las rutas
    TEST_ASSERT_EQUAL(initial_routes, get_num_routes());
    printf("Número final de rutas: %d\n", get_num_routes());

    teardown_custom_hook_test();
}


TEST_CASE("Test: route to Wi-Fi network", "[route]")
{   
    setup_custom_hook_test();

    // Se chequea el numero inicial de rutas
    unsigned initial_routes = get_num_routes();
    printf("Número inicial de rutas: %d\n", initial_routes);
    TEST_ASSERT_EQUAL(initial_routes, 1);

    // Se agregan las rutas
    ESP_ERROR_CHECK(add_route("192.190.1.1", SPI_GATEWAY, "255.255.0.0", "a"));
    ESP_ERROR_CHECK(add_route("192.168.1.100", WIFI_GATEWAY, "255.255.255.0", "b"));
    ESP_ERROR_CHECK(add_route("10.10.10.150", WIFI_GATEWAY, "255.255.255.0", "c"));
    
    struct netif *netif_;
    
    // Retrive the right interface -> SPI
    ip4_addr_t src_addr;
    ip4_addr_t dest_addr;

    // Convertir las cadenas de dirección IP a estructuras ip4_addr_t
    IP4_ADDR(&src_addr, 10, 0, 0, 1);  // 10.0.0.1
    IP4_ADDR(&dest_addr, 192, 168, 1, 25);  // 192.168.1.25

    // Llamar a la función con las estructuras ip4_addr_t
    struct netif * netif_route = custom_ip4_route_src_hook(&src_addr, &dest_addr);

    TEST_ASSERT_NOT_NULL(netif_route);
    
    // Check tx interface
    esp_netif_t * wifi_netif = get_wifi_netif();
    
    struct netif * wifi_netif_impl = esp_netif_get_netif_impl(wifi_netif);
    TEST_ASSERT_EQUAL(netif_route, wifi_netif_impl);

    // Check number of routes
    TEST_ASSERT_EQUAL(initial_routes + 3, get_num_routes());
    
    // Se eliminan las rutas
    ESP_ERROR_CHECK(rm_route("192.190.1.1", "255.255.0.0"));
    ESP_ERROR_CHECK(rm_route("192.168.1.100", "255.255.255.0"));
    ESP_ERROR_CHECK(rm_route("10.10.10.150", "255.255.255.0"));

    // Se chequea que se hayan eliminado las rutas
    TEST_ASSERT_EQUAL(initial_routes, get_num_routes());
    printf("Número final de rutas: %d\n", get_num_routes());

    teardown_custom_hook_test();
}

TEST_CASE("Test: remove non-existent route", "[route]") {
    setup_custom_hook_test();

    unsigned initial_routes = get_num_routes();

    // Attempt to remove a route that doesn't exist
    esp_err_t err = rm_route("192.168.99.1", "255.255.255.0");
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err); // Assuming your function returns this error

    // Verify that the number of routes remains the same
    TEST_ASSERT_EQUAL(initial_routes, get_num_routes());

    teardown_custom_hook_test();
}

TEST_CASE("Test: maximum number of routes", "[route]") {
    setup_custom_hook_test();

    unsigned initial_routes = get_num_routes();
    unsigned max_routes = MAX_NUMBER_ROUTES; // Ensure this function returns the maximum allowed routes

    // Add routes up to the maximum limit
    for (unsigned i = initial_routes; i < max_routes; ++i) {
        char ip[16];
        char key[32];
        snprintf(ip, sizeof(ip), "192.100.%d.1", i);
        snprintf(key, sizeof(key), "%u", i); // Generate a unique key for each route
        printf("ip: %s\n", ip);
        esp_err_t err = add_route(ip, SPI_GATEWAY, "255.255.255.0", key);
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }

    // Attempt to add one more route beyond the maximum
    esp_err_t err = add_route("192.100.255.1", SPI_GATEWAY, "255.255.255.0", "max_test_overflow");
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, err); // Adjust the error code based on your implementation

    // Clean up
    for (unsigned i = initial_routes; i < max_routes; ++i) {
        char ip[16];
        snprintf(ip, sizeof(ip), "192.100.%d.1", i);

        ESP_ERROR_CHECK(rm_route(ip, "255.255.255.0"));
    }

    TEST_ASSERT_EQUAL(initial_routes, get_num_routes());

    teardown_custom_hook_test();
}

TEST_CASE("Test: route selection based on priority and specificity", "[route]") {
    setup_custom_hook_test();

    // Add a general route
    ESP_ERROR_CHECK(add_route("0.0.0.0", WIFI_GATEWAY, "0.0.0.0", "default"));

    // Add a more specific route
    ESP_ERROR_CHECK(add_route("192.168.1.0", SPI_GATEWAY, "255.255.255.0", "specific"));

    ip4_addr_t src_addr;
    ip4_addr_t dest_addr;

    // Destination IP that matches the specific route
    IP4_ADDR(&src_addr, 10, 0, 0, 1);
    IP4_ADDR(&dest_addr, 192, 168, 1, 25);

    struct netif *netif_route = custom_ip4_route_src_hook(&src_addr, &dest_addr);
    TEST_ASSERT_NOT_NULL(netif_route);

    // Verify that the specific route is selected (SPI interface)
    esp_netif_t *spi_netif = get_ring_link_tx_netif();
    struct netif *spi_netif_impl = esp_netif_get_netif_impl(spi_netif);
    TEST_ASSERT_EQUAL(netif_route, spi_netif_impl);

    // Destination IP that matches only the default route
    IP4_ADDR(&dest_addr, 8, 8, 8, 8); // Google DNS

    netif_route = custom_ip4_route_src_hook(&src_addr, &dest_addr);
    TEST_ASSERT_NOT_NULL(netif_route);

    // Verify that the default route is selected (Wi-Fi interface)
    esp_netif_t *wifi_netif = get_wifi_netif();
    struct netif *wifi_netif_impl = esp_netif_get_netif_impl(wifi_netif);
    TEST_ASSERT_EQUAL(netif_route, wifi_netif_impl);

    // Clean up
    ESP_ERROR_CHECK(rm_route("0.0.0.0", "0.0.0.0"));
    ESP_ERROR_CHECK(rm_route("192.168.1.0", "255.255.255.0"));

    teardown_custom_hook_test();
}

TEST_CASE("Test: dynamic updates to routing table", "[route]") {
    setup_custom_hook_test();

    ip4_addr_t src_addr;
    ip4_addr_t dest_addr;

    IP4_ADDR(&src_addr, 10, 0, 0, 1);
    IP4_ADDR(&dest_addr, 172, 16, 0, 1);

    // Initially, no specific route to 172.16.0.1, so it should return the default SPI interface
    struct netif *netif_route = custom_ip4_route_src_hook(&src_addr, &dest_addr);
    TEST_ASSERT_NOT_NULL(netif_route);

    // Verify that the default interface (SPI) is returned
    esp_netif_t *spi_netif = get_ring_link_tx_netif();
    struct netif *spi_netif_impl = esp_netif_get_netif_impl(spi_netif);
    TEST_ASSERT_EQUAL(netif_route, spi_netif_impl);

    // Add a route to 172.16.0.0/16 via Wi-Fi
    ESP_ERROR_CHECK(add_route("172.16.0.0", WIFI_GATEWAY, "255.255.0.0", "dynamic"));

    // Now, the routing function should return the Wi-Fi interface for this destination
    netif_route = custom_ip4_route_src_hook(&src_addr, &dest_addr);
    TEST_ASSERT_NOT_NULL(netif_route);

    // Verify that the Wi-Fi interface is now selected
    esp_netif_t *wifi_netif = get_wifi_netif();
    struct netif *wifi_netif_impl = esp_netif_get_netif_impl(wifi_netif);
    TEST_ASSERT_EQUAL(netif_route, wifi_netif_impl);

    // Remove the route
    ESP_ERROR_CHECK(rm_route("172.16.0.0", "255.255.0.0"));

    // After removing the route, the default SPI interface should be selected again
    netif_route = custom_ip4_route_src_hook(&src_addr, &dest_addr);
    TEST_ASSERT_NOT_NULL(netif_route);

    // Verify that the default interface (SPI) is returned
    TEST_ASSERT_EQUAL(netif_route, spi_netif_impl);

    teardown_custom_hook_test();
}
