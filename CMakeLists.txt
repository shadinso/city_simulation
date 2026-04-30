cmake_minimum_required(VERSION 4.1)
project(myProj C)

set(CMAKE_C_STANDARD 99)

add_executable(myProj main.c
        dijkstra.h
        dijkstra.c
        graph.h
        graph.c)
