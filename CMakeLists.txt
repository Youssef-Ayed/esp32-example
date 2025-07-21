# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
if(EXISTS $ENV{IDF_PATH}/tools/cmake/test.cmake)
    include($ENV{IDF_PATH}/tools/cmake/test.cmake)
    add_subdirectory(test)
endif()
project(esp32-example)
# Include the test framework if available