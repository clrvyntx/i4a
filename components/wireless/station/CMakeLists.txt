idf_component_register(SRCS "station.c"
                    INCLUDE_DIRS "."
                    REQUIRES i4a_hal esp_netif client channel_manager traffic
                    )
