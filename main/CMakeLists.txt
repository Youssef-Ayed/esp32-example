idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES ota_plugin)
set(EXTRA_COMPONENT_DIRS ${CMAKE_SOURCE_DIR}/components)
