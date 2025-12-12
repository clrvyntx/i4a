idf_component_register(SRCS "access_point.c"
                    INCLUDE_DIRS "."
                    REQUIRES i4a_pysim esp_wifi esp_netif server traffic info_manager)

