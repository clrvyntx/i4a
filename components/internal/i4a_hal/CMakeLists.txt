idf_component_register(
    SRCS i4a_hal.c virtual_nic.c vnic_esp_glue.c
    INCLUDE_DIRS "include"
    REQUIRES "esp_wifi driver esp_netif"
)
