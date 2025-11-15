idf_component_register(SRCS 
     "ring_link_internal.c"
     "broadcast.c"
    INCLUDE_DIRS "include"
    REQUIRES ring_link_lowlevel esp_timer
)