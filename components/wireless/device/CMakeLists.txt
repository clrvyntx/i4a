idf_component_register(SRCS "device.c"
                    INCLUDE_DIRS "."
                    REQUIRES i4a_hal access_point station client server nvs_flash)
