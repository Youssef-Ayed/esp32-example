# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Include the test directory if it exists
if(EXISTS ${CMAKE_SOURCE_DIR}/test)
    set(EXTRA_COMPONENT_DIRS ${CMAKE_SOURCE_DIR}/test)
endif()

# Set the custom partition table
set(PARTITION_TABLE_SINGLE_APP_SIZE 0x100000)
set(PARTITION_TABLE_CSV_PATH ${CMAKE_CURRENT_SOURCE_DIR}/partitions.csv)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if(EXISTS "$ENV{IDF_PATH}/tools/cmake/test.cmake")
    include("$ENV{IDF_PATH}/tools/cmake/test.cmake")
endif()

project(esp32-example)
