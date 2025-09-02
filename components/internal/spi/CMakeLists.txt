idf_component_register(SRCS 
       "spi.c"
    INCLUDE_DIRS "include"
    REQUIRES driver config ring_link_lowlevel
)