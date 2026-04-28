cmake_minimum_required(VERSION 3.10)
project(city_simulation C)

set(CMAKE_C_STANDARD 11)

add_executable(city_simulation
        graph.c
        dijkstra.c
)