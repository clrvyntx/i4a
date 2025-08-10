add_library(i4a_wireless "include/wireless/wireless.h")
target_include_directories(i4a_wireless PUBLIC "include")
set_target_properties(i4a_wireless PROPERTIES LINKER_LANGUAGE C)