idf_component_register(
    SRCS pysim.c virtual_nic.c vnic_esp_glue.c uart.c
    INCLUDE_DIRS "include"
    REQUIRES "esp_wifi driver esp_netif"
)
