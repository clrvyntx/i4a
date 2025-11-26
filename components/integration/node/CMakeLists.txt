idf_component_register(SRCS "node.c"
                    INCLUDE_DIRS "."
                    REQUIRES config device ring_link callbacks channel_manager reset_manager internal_messages traffic info_manager)


