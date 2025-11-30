idf_component_register(
    SRCS "src/info_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES node ring_share traffic esp_http_client task_config
)

