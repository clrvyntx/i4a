cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS
    ${CMAKE_SOURCE_DIR}/../../components/wireless
    ${CMAKE_SOURCE_DIR}/../../components/internal
    ${CMAKE_SOURCE_DIR}/../../components/routing
    ${CMAKE_SOURCE_DIR}/../../components/integration
)

set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/../../components/sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(main)
