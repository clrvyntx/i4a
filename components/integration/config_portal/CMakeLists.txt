idf_component_register(
    SRCS "src/config_portal.c" "src/configuration_ap_filter.c" "src/captive_dns.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_http_server esp_hw_support esp_netif esp_timer esp_wifi
             lwip portal_settings reset_manager wifi_credentials task_config
)
