set(MAIN_SRCS
    "main.c"
    "nvs.c"
    "test_spi.c"
)

set(MAIN_INCLUDE_DIRS
    "include"
)

set(MAIN_REQUIRES
    config
    utils
    ring_link
    heartbeat
    wifi
    route
    nvs_flash
)

idf_component_register(
    SRCS
        ${MAIN_SRCS}
    INCLUDE_DIRS
        ${MAIN_INCLUDE_DIRS}
    REQUIRES
        ${MAIN_REQUIRES}
)

# idf_component_get_property(lwip lwip COMPONENT_LIB)
# target_compile_options(${lwip} PRIVATE "-I${PROJECT_DIR}/main/include")
# target_compile_definitions(${lwip} PRIVATE "-DESP_IDF_LWIP_HOOK_FILENAME=\"lwip_custom_hooks.h\"")
