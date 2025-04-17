idf_component_register(SRCS "route.c"
    INCLUDE_DIRS "include"
    REQUIRES ring_link_netif esp_wifi wifi)
