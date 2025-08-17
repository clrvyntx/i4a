#include "route.h"

static const char* TAG = "==> route";

static unsigned num_routes = 1;

unsigned get_num_routes(void){
    return num_routes;
}

esp_ip4_addr_t get_ip_gateway(gateway_t gw){
    esp_ip4_addr_t ip_addr;
    if (gw == SPI_GATEWAY) {
        ip_addr = get_spi_tx_ip_interface_address();
    } else {
        ip_addr = get_wifi_ip_interface_address();
    }
    return ip_addr;
}

esp_err_t add_route(char * ip, gateway_t gw, char * netmask, char * key)
{
    esp_netif_t *new_route;
    esp_ip4_addr_t ip_addr;
    esp_ip4_addr_t mask_addr;
    esp_ip4_addr_t gw_addr;
    
    if ( num_routes >= MAX_NUMBER_ROUTES ){
        return ESP_ERR_NO_MEM;
    }

    gw_addr = get_ip_gateway(gw);
    esp_netif_str_to_ip4(ip, &ip_addr);
    esp_netif_str_to_ip4(netmask, &mask_addr);
    
    void custom_input(void *h, void *buffer, size_t len, void *l2_buff){}

    err_t custom_init(struct netif *netif){
        return ERR_OK;
    }
    const esp_netif_netstack_config_t s_spi_netif_config = {
        .lwip = {
            .init_fn = custom_init,
            .input_fn = custom_input
        }};
    
    const esp_netif_ip_info_t ip_info = {
        .ip = ip_addr,
        .gw = gw_addr,
        .netmask = mask_addr,
    };
    esp_netif_inherent_config_t base_netif_config = {
        .flags = ESP_NETIF_FLAG_AUTOUP,
        .ip_info = &ip_info,
        .if_key = key,
        .if_desc = key,
        .route_prio = 15,
        .bridge_info = NULL};
    esp_err_t custom_driver_transmit(void *h, void *buffer, size_t len)
    {
        return ESP_OK;
    }

    esp_err_t custom_handle(void *h, void *buffer, size_t len)
    {
        return ESP_OK;
    }

    esp_netif_driver_ifconfig_t driver_ifconfig = {
        .transmit = custom_driver_transmit,
        .handle = custom_handle
    };
    esp_netif_config_t netif_config = {
        .base = &base_netif_config,
        .stack = &s_spi_netif_config,
        .driver = &driver_ifconfig};
    new_route = esp_netif_new(&netif_config);

    if (new_route == NULL) {
        ESP_LOGE(TAG, "esp_netif_new failed!");
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t mac[6] = {0x11, 0x11, 0x22, 0x33, 0x44, 0x01};
    mac[5] = num_routes & 0xFF;
    esp_netif_set_mac(new_route, mac);
    esp_netif_action_start(new_route, NULL, 0, NULL);
    num_routes++;
    ESP_LOGI(TAG, "New route added");
    return ESP_OK;
}


esp_err_t rm_route(char * ip, char * netmask){
    struct netif *netif;
    esp_netif_t *route_netif;
    esp_ip4_addr_t ip_addr;
    esp_ip4_addr_t mask_addr;

    esp_netif_str_to_ip4(ip, &ip_addr);
    esp_netif_str_to_ip4(netmask, &mask_addr);
    for (netif = netif_list; netif != NULL; netif = netif->next) {
        if(ip4_addr_cmp(&ip_addr, netif_ip4_addr(netif)) && ip4_addr_cmp(&mask_addr, netif_ip4_netmask(netif)))
        {
            route_netif = esp_netif_get_handle_from_netif_impl(netif);
            if(!route_netif){
                break;
            }
            esp_netif_action_stop(route_netif, NULL, 0, NULL);
            esp_netif_destroy(route_netif);
            num_routes--;
            ESP_LOGI(TAG, "Route removed");
            return ESP_OK;
        }
    }
    ESP_LOGW(TAG, "Could not remove route");
    return ESP_ERR_NOT_FOUND;
}

