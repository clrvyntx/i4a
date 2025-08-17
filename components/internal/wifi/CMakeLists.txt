idf_component_register(SRCS "wifi.c"
    INCLUDE_DIRS "include"
    REQUIRES config esp_wifi lwip_custom_hooks esp_event
)
