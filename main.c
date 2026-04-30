#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"
#include "graph.h"
#include "dijkstra.h"
#include "renderer.h"

int main(int argc, char *argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return 1;
    }

    Graph *g = NULL;
    int src = 0, dst = 0;

    if (read_graph_from_file(argv[1], &g, &src, &dst) != 0)
        return 1;

    NodeInfo *info = (NodeInfo *)malloc(g->num_nodes * sizeof(NodeInfo));
    if (!info) {
        fprintf(stderr, "Error: malloc failed\n");
        free_graph(g);
        return 1;
    }
    build_city_layout(g, info);

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "City Traffic Simulation");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(CLITERAL(Color){ 34, 40, 49, 255 });
        draw_graph(g, info);
        EndDrawing();
    }

    CloseWindow();
    free(info);
    free_graph(g);

    return 0;
}