add_library(i4a_siblings "include/siblings/siblings.h")
target_include_directories(i4a_siblings PUBLIC "include")
set_target_properties(i4a_siblings PROPERTIES LINKER_LANGUAGE C)