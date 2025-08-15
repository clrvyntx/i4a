idf_component_register(SRCS "server.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi nvs_flash esp_event)
