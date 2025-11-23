idf_component_register(
    SRCS "config.c"
    INCLUDE_DIRS "include"
    REQUIRES "driver esp_netif" i4a_hal
)
