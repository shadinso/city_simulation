#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <math.h>


#include "raylib.h"
#include "graph.h"
#include "dijkstra.h"
#include "renderer.h"

#define MAX_TRAVELERS 20
#define USEC_PER_WEIGHT 1500000
#define NODE_EDGE_OFFSET 45.0f

typedef struct {
    int src;
    int dst;
    pid_t pid;

    Color color;
    int finished;

    int *path;
    int path_len;
    int path_index;

    float x, y;
    float from_x, from_y;
    float to_x, to_y;
    float move_progress;
    float move_duration;
    int is_moving;
} Traveler;

static int get_edge_weight(Graph *g, int from, int to) {
    Edge *e = g->nodes[from].head;
    while (e) {
        if (e->dest == to) return e->weight;
        e = e->next;
    }
    return 1;
}

static int read_input_file(const char *filename,
                           Graph **out_graph,
                           Traveler travelers[],
                           int *traveler_count) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open file %s\n", filename);
        return -1;
    }

    int N, M;
    if (fscanf(fp, "%d %d", &N, &M) != 2) {
        fclose(fp);
        return -1;
    }

    Graph *g = create_graph(N, M);

    for (int i = 0; i < M; i++) {
        int src, dst, w;
        fscanf(fp, "%d %d %d", &src, &dst, &w);
        add_edge(g, src, dst, w);
    }

    int count;
    fscanf(fp, "%d", &count);

    if (count > MAX_TRAVELERS)
        count = MAX_TRAVELERS;

    *traveler_count = count;

    for (int i = 0; i < count; i++) {
        fscanf(fp, "%d %d", &travelers[i].src, &travelers[i].dst);

        travelers[i].pid = -1;
        travelers[i].finished = 0;
        travelers[i].path = NULL;
        travelers[i].path_len = 0;
        travelers[i].path_index = 0;
        travelers[i].x = 0;
        travelers[i].y = 0;
        travelers[i].from_x = 0;
        travelers[i].from_y = 0;
        travelers[i].to_x = 0;
        travelers[i].to_y = 0;
        travelers[i].move_progress = 1.0f;
        travelers[i].move_duration = 1.0f;
        travelers[i].is_moving = 0;
    }

    fclose(fp);
    *out_graph = g;
    return 0;
}

static void start_next_move(Traveler *t, Graph *g, NodeInfo *info) {
    if (t->path_index >= t->path_len - 1) {
        t->finished = 1;

        if (t->pid > 0) {
            kill(t->pid, SIGTERM);
            waitpid(t->pid, NULL, 0);
            printf("[PID=%d] finished\n", t->pid);
            fflush(stdout);
            t->pid = -1;
        }

        return;
    }

    int from = t->path[t->path_index];
    int to = t->path[t->path_index + 1];

    float x1 = info[from].x;
    float y1 = info[from].y;
    float x2 = info[to].x;
    float y2 = info[to].y;

    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len == 0) len = 1;

    float ux = dx / len;
    float uy = dy / len;

    t->from_x = x1 + ux * NODE_EDGE_OFFSET;
    t->from_y = y1 + uy * NODE_EDGE_OFFSET;
    t->to_x = x2 - ux * NODE_EDGE_OFFSET;
    t->to_y = y2 - uy * NODE_EDGE_OFFSET;

    t->x = t->from_x;
    t->y = t->from_y;

    int w = get_edge_weight(g, from, to);
    if (w <= 0) w = 1;

    t->move_duration = (float)(w * USEC_PER_WEIGHT) / 1000000.0f;
    t->move_progress = 0.0f;
    t->is_moving = 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return 1;
    }

    Graph *g = NULL;
    Traveler travelers[MAX_TRAVELERS];
    int traveler_count = 0;

    if (read_input_file(argv[1], &g, travelers, &traveler_count) != 0)
        return 1;

    NodeInfo *info = malloc(g->num_nodes * sizeof(NodeInfo));
    if (!info) {
        free_graph(g);
        return 1;
    }

    build_layout(g, info);

    Color colors[MAX_TRAVELERS] = {
        RED, BLUE, GREEN, ORANGE, PURPLE,
        YELLOW, PINK, SKYBLUE, LIME, MAROON,
        GOLD, VIOLET, BROWN, BEIGE, MAGENTA,
        DARKGREEN, DARKBLUE, DARKPURPLE, DARKBROWN, RAYWHITE
    };

    for (int i = 0; i < traveler_count; i++) {
        travelers[i].color = colors[i % MAX_TRAVELERS];

        int cost = dijkstra(g,
                            travelers[i].src,
                            travelers[i].dst,
                            &travelers[i].path,
                            &travelers[i].path_len);

        if (cost < 0 || travelers[i].path_len <= 0) {
            travelers[i].finished = 1;
            continue;
        }

        travelers[i].x = info[travelers[i].src].x;
        travelers[i].y = info[travelers[i].src].y;

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            travelers[i].finished = 1;
        } else if (pid == 0) {
            printf("[%d] started\n", getpid());
            fflush(stdout);

            while (1) {
                pause();
            }

            exit(0);
        } else {
            travelers[i].pid = pid;
        }
    }

    for (int i = 0; i < traveler_count; i++) {
        if (!travelers[i].finished) {
            start_next_move(&travelers[i], g, info);
        }
    }

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "City Simulation - Milestone 4");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int all_done = 1;

        for (int i = 0; i < traveler_count; i++) {
            if (travelers[i].finished)
                continue;

            all_done = 0;

            if (travelers[i].is_moving) {
                travelers[i].move_progress += dt / travelers[i].move_duration;

                if (travelers[i].move_progress >= 1.0f) {
                    travelers[i].move_progress = 1.0f;
                    travelers[i].x = travelers[i].to_x;
                    travelers[i].y = travelers[i].to_y;
                    travelers[i].is_moving = 0;

                    travelers[i].path_index++;
                    start_next_move(&travelers[i], g, info);
                } else {
                    float t = travelers[i].move_progress;

                    travelers[i].x =
                        travelers[i].from_x +
                        (travelers[i].to_x - travelers[i].from_x) * t;

                    travelers[i].y =
                        travelers[i].from_y +
                        (travelers[i].to_y - travelers[i].from_y) * t;
                }
            }
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);

        draw_graph(g, info);

        for (int i = 0; i < traveler_count; i++) {
            if (travelers[i].finished)
                continue;

            DrawCircle((int)travelers[i].x,
                       (int)travelers[i].y,
                       13,
                       travelers[i].color);

            DrawCircleLines((int)travelers[i].x,
                            (int)travelers[i].y,
                            13,
                            BLACK);

            char label[16];
            sprintf(label, "T%d", i + 1);

            DrawText(label,
                     (int)travelers[i].x + 15,
                     (int)travelers[i].y - 8,
                     16,
                     WHITE);
        }

        DrawText("City Simulation - Milestone 4",
                 20,
                 20,
                 24,
                 RAYWHITE);

        DrawText("Parent computes paths | Children print started and wait",
                 20,
                 SCREEN_HEIGHT - 35,
                 16,
                 RAYWHITE);

        if (all_done) {
            DrawText("ALL FINISHED!",
                     20,
                     60,
                     28,
                     GREEN);
        }

        EndDrawing();

        if (all_done) {
            sleep(1);
            break;
        }
    }

    for (int i = 0; i < traveler_count; i++) {
        if (travelers[i].pid > 0) {
            kill(travelers[i].pid, SIGTERM);
            waitpid(travelers[i].pid, NULL, 0);
        }

        free(travelers[i].path);
    }

    free(info);
    free_graph(g);
    CloseWindow();

    return 0;
}
