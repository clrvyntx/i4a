idf_component_register(SRCS 
     "heartbeat.c"
    INCLUDE_DIRS "include"
    REQUIRES ring_link_internal esp_timer
)