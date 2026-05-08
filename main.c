#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"
#include "graph.h"
#include "dijkstra.h"
#include "renderer.h"

static int get_edge_weight(Graph *g, int from, int to) {

    Edge *e = g->nodes[from].head;

    while (e) {

        if (e->dest == to)
            return e->weight;

        e = e->next;
    }

    return 1;
}

int main(int argc, char *argv[]) {

    if (argc < 2) {

        fprintf(stderr,
                "Usage: %s <graph_file>\n",
                argv[0]);

        return 1;
    }

    Graph *g = NULL;

    int src = 0;
    int dst = 0;

    if (read_graph_from_file(
            argv[1],
            &g,
            &src,
            &dst) != 0) {

        return 1;
    }

    NodeInfo *info =
        malloc(g->num_nodes * sizeof(NodeInfo));

    build_city_layout(g, info);

    int *path = NULL;
    int path_len = 0;

    int cost =
        dijkstra(g,
                 src,
                 dst,
                 &path,
                 &path_len);

    if (cost < 0) {

        printf("No path found\n");

        return 1;
    }

    InitWindow(
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        "City Simulation"
    );

    SetTargetFPS(60);

    int playing = 0;

    int current_segment = 0;
    int current_step = 0;

    float timer = 0;

    float vehicle_x = info[path[0]].x;
    float vehicle_y = info[path[0]].y;

    int waiting = 0;
    float wait_timer = 0;

    while (!WindowShouldClose()) {

        Rectangle btn = {
            20,
            20,
            140,
            40
        };

        if (IsMouseButtonPressed(
                MOUSE_LEFT_BUTTON)
            &&
            CheckCollisionPointRec(
                GetMousePosition(),
                btn)) {

            playing = !playing;
        }

        if (playing &&
            current_segment < path_len - 1) {

            if (waiting) {

                wait_timer += GetFrameTime();

                if (wait_timer >= 1.0f) {

                    waiting = 0;
                    wait_timer = 0;
                }

            } else {

                int from =
                    path[current_segment];

                int to =
                    path[current_segment + 1];

                int weight =
                    get_edge_weight(
                        g,
                        from,
                        to);

                int total_steps = weight;

                timer += GetFrameTime();

                if (timer >= 0.3f) {

                    timer = 0;

                    current_step++;

                    float t =
                        (float)current_step
                        / total_steps;

                    vehicle_x =
                        info[from].x +
                        (info[to].x
                         - info[from].x)
                        * t;

                    vehicle_y =
                        info[from].y +
                        (info[to].y
                         - info[from].y)
                        * t;

                    if (current_step
                        >= total_steps) {

                        current_segment++;

                        current_step = 0;

                        vehicle_x =
                            info[to].x;

                        vehicle_y =
                            info[to].y;

                        if (current_segment
                            < path_len - 1) {

                            waiting = 1;
                        }
                    }
                }
            }
        }

        BeginDrawing();

        ClearBackground(DARKGRAY);

        draw_graph(g, info);

        draw_vehicle(
            vehicle_x,
            vehicle_y
        );

        DrawRectangleRec(
            btn,
            LIGHTGRAY
        );

        if (playing) {

            DrawText(
                "STOP",
                50,
                30,
                20,
                RED
            );

        } else {

            DrawText(
                "PLAY",
                50,
                30,
                20,
                GREEN
            );
        }

        if (current_segment
            >= path_len - 1) {

            DrawText(
                "ARRIVED!",
                20,
                80,
                30,
                GREEN
            );
        }

        EndDrawing();
    }

    free(path);
    free(info);
    free_graph(g);

    CloseWindow();

    return 0;
}