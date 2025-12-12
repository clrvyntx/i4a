idf_component_register(SRCS 
    spi_link.c netif_glue.c broadcast.c spi_hal.c
    INCLUDE_DIRS "include"
    REQUIRES i4a_pysim esp_netif driver callbacks
)
