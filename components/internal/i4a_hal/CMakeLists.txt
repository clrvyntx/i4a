idf_component_register(
    SRCS qemu/qemu.c qemu/virtual_nic.c qemu/vnic_esp_glue.c
    INCLUDE_DIRS "include"
    REQUIRES "esp_wifi driver esp_netif"
)
