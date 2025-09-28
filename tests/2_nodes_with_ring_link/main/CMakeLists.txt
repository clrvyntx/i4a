idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES node callbacks siblings ring_share channel_manager)

idf_component_get_property(lwip lwip COMPONENT_LIB)
target_compile_options(${lwip} PRIVATE "-I${CMAKE_SOURCE_DIR}/../../components/internal/lwip_custom_hooks")
target_compile_definitions(${lwip} PRIVATE "-DESP_IDF_LWIP_HOOK_FILENAME=\"lwip_custom_hooks.h\"")
