idf_component_register(
    SRCS
        "src/shared_state.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        config
        ring_share
        sync
)
