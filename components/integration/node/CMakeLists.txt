idf_component_register(SRCS "node.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_timer config device spi_link callbacks channel_manager reset_manager internal_messages traffic info_manager)


