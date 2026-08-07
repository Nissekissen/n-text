cmake_minimum_required(VERSION 3.21)
project(n_text, C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(n_text src/main.c)
target_sources(n_text PRIVATE src/renderer.c src/rope.c src/cursor.c)
set_target_properties(n_text PROPERTIES OUTPUT_NAME "ntxt")

install(TARGETS n_text RUNTIME DESTINATION bin)

