cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS ${CMAKE_SOURCE_DIR}/../../components/wireless ${CMAKE_SOURCE_DIR}/../../components/internal)
project(main)
