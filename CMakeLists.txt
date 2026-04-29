cmake_minimum_required(VERSION 3.15)
project(tanker111 C)

set(CMAKE_C_STANDARD 99)

add_executable(tanker111
        tanker_counter111.c
        lib/lodepng.c
)

target_link_libraries(tanker111 m)

# Добавляем папку с заголовочными файлами
target_include_directories(tanker111 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib)