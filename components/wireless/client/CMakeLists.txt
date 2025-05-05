idf_component_register(SRCS "client.c"
                                INCLUDE_DIRS "."
                                REQUIRES esp_event esp_netif esp_wifi)
