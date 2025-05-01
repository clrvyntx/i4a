idf_component_register(
    SRCS
        "src/ring_link.c"
        "src/ring_link_lowlevel.c"
        "src/ring_link_payload.c"
        "src/broadcast.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        config
        spi
)

