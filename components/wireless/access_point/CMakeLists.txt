idf_component_register(SRCS "access_point.c"
                    INCLUDE_DIRS "."
                    REQUIRES i4a_hal esp_netif server traffic)

set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY LINK_INTERFACE_MULTIPLICITY 5)
