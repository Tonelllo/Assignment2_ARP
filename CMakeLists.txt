cmake_minimum_required(VERSION 3.6)

project("Assignment2_ARP_ConTo")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

add_compile_options(-Wall -Wextra -Wpedantic)

add_subdirectory(headers)
add_subdirectory(src)
