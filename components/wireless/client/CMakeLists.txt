idf_component_register(SRCS "client.c"
                                INCLUDE_DIRS "."
                                REQUIRES esp_netif callbacks task_config)

set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY LINK_INTERFACE_MULTIPLICITY 3)
