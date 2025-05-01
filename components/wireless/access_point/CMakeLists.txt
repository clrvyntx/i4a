idf_component_register(SRCS "access_point.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi nvs_flash esp_event server)
