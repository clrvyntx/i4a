idf_component_register(
    SRCS "src/channel_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES node routing_config ring_share
)
