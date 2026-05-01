cmake_minimum_required(VERSION 3.10)
project(city_simulation C)

set(CMAKE_C_STANDARD 99)

add_executable(city_simulation
        main.c
        graph.c
        dijkstra.c
        renderer.c
)

target_link_libraries(city_simulation
        raylib
        m
        dl
        pthread
        X11
)

configure_file(city.txt city.txt COPYONLY)  