idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "."
                       REQUIRES unity
                                lwip_custom_hooks
                                route
                                freertos
                                esp_common
                                esp_system
                                nvs_flash
                                test_utils
                                ring_link
                                wifi
                                )