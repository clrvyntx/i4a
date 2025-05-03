idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES device config ring_link wifi utils client)
