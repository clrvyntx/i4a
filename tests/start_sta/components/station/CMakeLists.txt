idf_component_register(SRCS "utils.c" "station.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi nvs_flash esp_event esp_netif client
                    )
