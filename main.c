#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include "raylib.h"
#include "graph.h"
#include "dijkstra.h"
#include "renderer.h"

#define MAX_TRAVELERS 20

typedef struct {
    int src;
    int dst;
    int *path;
    int path_len;
    int cost;
    pid_t pid;
    Color color;

    int active;
    int finished;
    int current_segment;
    int current_step;
    float timer;
    int waiting;
    float wait_timer;
    float x;
    float y;
} Traveler;

static int get_edge_weight(Graph *g, int from, int to) {
    Edge *e = g->nodes[from].head;
    while (e) {
        if (e->dest == to) return e->weight;
        e = e->next;
    }
    return 1;
}

static int read_milestone4_file(const char *filename, Graph **out_graph,
                                Traveler travelers[], int *traveler_count) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open file '%s'\n", filename);
        return -1;
    }

    int N, M;
    if (fscanf(fp, "%d %d", &N, &M) != 2 || N <= 0 || M < 0) {
        fprintf(stderr, "Error: invalid graph header\n");
        fclose(fp);
        return -1;
    }

    Graph *g = create_graph(N, M);
    if (!g) {
        fclose(fp);
        return -1;
    }

    for (int i = 0; i < M; i++) {
        int src, dst, w;
        if (fscanf(fp, "%d %d %d", &src, &dst, &w) != 3) {
            fprintf(stderr, "Error: malformed edge\n");
            free_graph(g);
            fclose(fp);
            return -1;
        }

        if (src < 0 || src >= N || dst < 0 || dst >= N) {
            fprintf(stderr, "Error: node index out of range\n");
            free_graph(g);
            fclose(fp);
            return -1;
        }

        if (w < 0) {
            fprintf(stderr, "Error: negative weight (%d) is not allowed\n", w);
            free_graph(g);
            fclose(fp);
            return -1;
        }

        add_edge(g, src, dst, w);
    }

    int count;
    if (fscanf(fp, "%d", &count) != 1 || count <= 0 || count > MAX_TRAVELERS) {
        fprintf(stderr, "Error: invalid travelers count\n");
        free_graph(g);
        fclose(fp);
        return -1;
    }

    for (int i = 0; i < count; i++) {
        int src, dst;
        if (fscanf(fp, "%d %d", &src, &dst) != 2) {
            fprintf(stderr, "Error: malformed traveler\n");
            free_graph(g);
            fclose(fp);
            return -1;
        }

        if (src < 0 || src >= N || dst < 0 || dst >= N) {
            fprintf(stderr, "Error: traveler node out of range\n");
            free_graph(g);
            fclose(fp);
            return -1;
        }

        travelers[i].src = src;
        travelers[i].dst = dst;
        travelers[i].path = NULL;
        travelers[i].path_len = 0;
        travelers[i].cost = -1;
        travelers[i].pid = -1;
        travelers[i].active = 1;
        travelers[i].finished = 0;
        travelers[i].current_segment = 0;
        travelers[i].current_step = 0;
        travelers[i].timer = 0;
        travelers[i].waiting = 0;
        travelers[i].wait_timer = 0;
    }

    fclose(fp);
    *out_graph = g;
    *traveler_count = count;
    return 0;
}

static void update_traveler(Graph *g, NodeInfo *info, Traveler *t) {
    if (!t->active || t->finished || t->path_len <= 0) return;

    if (t->current_segment >= t->path_len - 1) {
        t->finished = 1;
        if (t->pid > 0) kill(t->pid, SIGTERM);
        return;
    }

    if (t->waiting) {
        t->wait_timer += GetFrameTime();
        if (t->wait_timer >= 1.0f) {
            t->waiting = 0;
            t->wait_timer = 0;
        }
        return;
    }

    int from = t->path[t->current_segment];
    int to = t->path[t->current_segment + 1];
    int weight = get_edge_weight(g, from, to);

    if (weight <= 0) weight = 1;

    t->timer += GetFrameTime();

    if (t->timer >= 0.3f) {
        t->timer = 0;
        t->current_step++;

        float ratio = (float)t->current_step / weight;

        t->x = info[from].x + (info[to].x - info[from].x) * ratio;
        t->y = info[from].y + (info[to].y - info[from].y) * ratio;

        if (t->current_step >= weight) {
            t->current_segment++;
            t->current_step = 0;

            t->x = info[to].x;
            t->y = info[to].y;

            if (t->current_segment < t->path_len - 1) {
                t->waiting = 1;
            } else {
                t->finished = 1;
                if (t->pid > 0) kill(t->pid, SIGTERM);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return 1;
    }

    Graph *g = NULL;
    Traveler travelers[MAX_TRAVELERS];
    int traveler_count = 0;

    if (read_milestone4_file(argv[1], &g, travelers, &traveler_count) != 0) {
        return 1;
    }

    NodeInfo *info = malloc(g->num_nodes * sizeof(NodeInfo));
    if (!info) {
        free_graph(g);
        return 1;
    }

    build_city_layout(g, info);

    Color colors[MAX_TRAVELERS] = {
        RED, BLUE, GREEN, ORANGE, PURPLE,
        YELLOW, PINK, SKYBLUE, LIME, MAROON,
        GOLD, VIOLET, BROWN, BEIGE, MAGENTA,
        DARKGREEN, DARKBLUE, DARKPURPLE, DARKBROWN, RAYWHITE
    };

    for (int i = 0; i < traveler_count; i++) {
        travelers[i].color = colors[i % MAX_TRAVELERS];

        travelers[i].cost = dijkstra(g,
                                     travelers[i].src,
                                     travelers[i].dst,
                                     &travelers[i].path,
                                     &travelers[i].path_len);

        if (travelers[i].cost < 0) {
            printf("No path found for traveler %d\n", i);
            travelers[i].active = 0;
            continue;
        }

        travelers[i].x = info[travelers[i].path[0]].x;
        travelers[i].y = info[travelers[i].path[0]].y;

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            travelers[i].active = 0;
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

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "City Simulation - Milestone 4");
    SetTargetFPS(60);

    int playing = 0;

    while (!WindowShouldClose()) {
        Rectangle btn = {20, 20, 140, 40};

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointRec(GetMousePosition(), btn)) {
            playing = !playing;
        }

        if (playing) {
            for (int i = 0; i < traveler_count; i++) {
                update_traveler(g, info, &travelers[i]);
            }
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);

        draw_graph(g, info);

        for (int i = 0; i < traveler_count; i++) {
            if (travelers[i].active) {
                DrawCircle((int)travelers[i].x,
                           (int)travelers[i].y,
                           10,
                           travelers[i].color);

                char label[10];
                sprintf(label, "T%d", i + 1);
                DrawText(label,
                         (int)travelers[i].x + 12,
                         (int)travelers[i].y - 8,
                         14,
                         WHITE);
            }
        }

        DrawRectangleRec(btn, LIGHTGRAY);

        if (playing) {
            DrawText("STOP", 50, 30, 20, RED);
        } else {
            DrawText("PLAY", 50, 30, 20, GREEN);
        }

        int all_finished = 1;
        for (int i = 0; i < traveler_count; i++) {
            if (travelers[i].active && !travelers[i].finished) {
                all_finished = 0;
            }
        }

        if (all_finished) {
            DrawText("ALL ARRIVED!", 20, 80, 30, GREEN);
        }

        EndDrawing();
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
