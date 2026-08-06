cmake_minimum_required(VERSION 3.21)
project(n_text, C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(cJSON CONFIG REQUIRED)

add_executable(n_text src/main.c)
target_sources(n_text PRIVATE src/renderer.c src/rope.c src/cursor.c)
target_link_libraries(n_text PRIVATE cjson)

