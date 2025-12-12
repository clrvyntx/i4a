idf_component_register(
    SRCS i4a_pysim.c virtual_nic.c vnic_esp_glue.c
    INCLUDE_DIRS "include"
    REQUIRES "pysim esp_wifi esp_netif"
)
