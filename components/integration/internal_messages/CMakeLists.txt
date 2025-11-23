idf_component_register(SRCS "internal_messages.c"
                    INCLUDE_DIRS "."
                    REQUIRES ring_share callbacks channel_manager reset_manager info_manager)
