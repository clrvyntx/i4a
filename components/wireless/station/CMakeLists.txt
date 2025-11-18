idf_component_register(SRCS "station.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_netif client channel_manager traffic
                    )
