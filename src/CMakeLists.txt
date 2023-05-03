# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS ${app_sources})

target_add_binary_data(${COMPONENT_TARGET} "../webui/index.html.gz" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/main.css.gz" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/main.js.gz" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/md5.min.js.gz" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/pico.min.css.gz" BINARY)