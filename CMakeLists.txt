cmake_minimum_required(VERSION 3.10)

project(city_simulation C)

set(CMAKE_C_STANDARD 99)

add_executable(dijkstra
        dijkstra_main.c
        graph.c
        dijkstra.c
)

add_executable(sim
        main.c
        graph.c
        dijkstra.c
        renderer.c
)

add_executable(sim_m4
        main_m4.c
        graph.c
        dijkstra.c
        renderer.c
)

target_link_libraries(sim
        raylib
        m
        dl
        pthread
        X11
)

target_link_libraries(sim_m4
        raylib
        m
        dl
        pthread
        X11
)

configure_file(city.txt city.txt COPYONLY)

add_executable(sim_m7
        main_m7.c
        graph.c
        dijkstra.c
        renderer.c
)

target_link_libraries(sim_m7
        raylib
        m
        dl
        pthread
        X11
)


