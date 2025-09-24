idf_component_register(SRCS "device.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi nvs_flash access_point station client server)
