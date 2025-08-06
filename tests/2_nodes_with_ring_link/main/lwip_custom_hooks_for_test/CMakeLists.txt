idf_component_register(
    SRCS "lwip_custom_hooks.c"
    INCLUDE_DIRS "include"
    REQUIRES lwip device ring_link_netif
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE "-DLWIP_HOOK_IP4_ROUTE_SRC=custom_ip4_route_src_hook")
