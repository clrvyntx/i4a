idf_component_register(SRCS 
       "ring_link_lowlevel.c"
       "ring_link_payload.c"      
    INCLUDE_DIRS "include"
    REQUIRES driver config spi physim
)