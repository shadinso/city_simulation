#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#include "raylib.h"
#include "graph.h"
#include "dijkstra.h"
#include "renderer.h"

#define MAX_TRAVELERS 20

#define MSG_NODE 1
#define MSG_FINISHED 2

typedef struct {
    int type;
    pid_t pid;
    int traveler_index;
    int current_node;
    int next_node;
} IPCMessage;

typedef struct {
    int src;
    int dst;
    pid_t pid;
    int pipe_fd[2];

    Color color;
    int active;
    int finished;

    int current_node;
    int next_node;

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

static int read_milestone5_file(const char *filename, Graph **out_graph,
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
        travelers[i].pid = -1;
        travelers[i].pipe_fd[0] = -1;
        travelers[i].pipe_fd[1] = -1;
        travelers[i].active = 1;
        travelers[i].finished = 0;
        travelers[i].current_node = src;
        travelers[i].next_node = -1;
        travelers[i].x = 0;
        travelers[i].y = 0;
    }

    fclose(fp);

    *out_graph = g;
    *traveler_count = count;
    return 0;
}

static void send_message(int fd, int type, int index, int current_node, int next_node) {
    IPCMessage msg;
    msg.type = type;
    msg.pid = getpid();
    msg.traveler_index = index;
    msg.current_node = current_node;
    msg.next_node = next_node;

    write(fd, &msg, sizeof(msg));
}

static void child_process(Graph *g, Traveler t, int index, int write_fd) {
    int *path = NULL;
    int path_len = 0;

    int cost = dijkstra(g, t.src, t.dst, &path, &path_len);

    if (cost < 0 || path_len <= 0) {
        send_message(write_fd, MSG_FINISHED, index, t.src, -1);
        close(write_fd);
        free(path);
        exit(0);
    }

    for (int i = 0; i < path_len; i++) {
        int current = path[i];
        int next = -1;

        if (i < path_len - 1) {
            next = path[i + 1];
        }

        send_message(write_fd, MSG_NODE, index, current, next);

        if (i == path_len - 1) {
            break;
        }

        int weight = get_edge_weight(g, current, next);
        if (weight <= 0) weight = 1;

        usleep(weight * 300000);

        if (i > 0 && i < path_len - 1) {
            sleep(1);
        }
    }

    send_message(write_fd, MSG_FINISHED, index, path[path_len - 1], -1);

    close(write_fd);
    free(path);
    exit(0);
}

static void handle_message(IPCMessage msg, Traveler travelers[], NodeInfo *info) {
    int i = msg.traveler_index;

    if (msg.type == MSG_NODE) {
        travelers[i].current_node = msg.current_node;
        travelers[i].next_node = msg.next_node;

        travelers[i].x = info[msg.current_node].x;
        travelers[i].y = info[msg.current_node].y;

        if (msg.next_node == -1) {
            printf("[PID=%d] arrived at node %d | DESTINATION\n",
                   msg.pid, msg.current_node);
        } else {
            printf("[PID=%d] arrived at node %d | next node: %d\n",
                   msg.pid, msg.current_node, msg.next_node);
        }

        fflush(stdout);
    }

    if (msg.type == MSG_FINISHED) {
        travelers[i].finished = 1;
        travelers[i].active = 0;

        printf("[PID=%d] finished\n", msg.pid);
        fflush(stdout);
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

    if (read_milestone5_file(argv[1], &g, travelers, &traveler_count) != 0) {
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
        travelers[i].x = info[travelers[i].src].x;
        travelers[i].y = info[travelers[i].src].y;

        if (pipe(travelers[i].pipe_fd) == -1) {
            perror("pipe");
            travelers[i].active = 0;
            continue;
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            travelers[i].active = 0;
            close(travelers[i].pipe_fd[0]);
            close(travelers[i].pipe_fd[1]);
        } else if (pid == 0) {
            close(travelers[i].pipe_fd[0]);
            child_process(g, travelers[i], i, travelers[i].pipe_fd[1]);
        } else {
            travelers[i].pid = pid;
            close(travelers[i].pipe_fd[1]);

            int flags = fcntl(travelers[i].pipe_fd[0], F_GETFL, 0);
            fcntl(travelers[i].pipe_fd[0], F_SETFL, flags | O_NONBLOCK);
        }
    }

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "City Simulation - Milestone 5 IPC");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        for (int i = 0; i < traveler_count; i++) {
            if (travelers[i].pipe_fd[0] != -1) {
                IPCMessage msg;
                ssize_t bytes;

                while ((bytes = read(travelers[i].pipe_fd[0], &msg, sizeof(msg))) == sizeof(msg)) {
                    handle_message(msg, travelers, info);
                }
            }
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);

        draw_graph(g, info);

        for (int i = 0; i < traveler_count; i++) {
            if (!travelers[i].finished) {
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

        int all_finished = 1;
        for (int i = 0; i < traveler_count; i++) {
            if (!travelers[i].finished) {
                all_finished = 0;
            }
        }

        if (all_finished) {
            DrawText("ALL FINISHED!", 20, 30, 30, GREEN);
        }

        DrawText("Milestone 5 - IPC using pipes", 20, SCREEN_HEIGHT - 35, 20, RAYWHITE);

        EndDrawing();
    }

    for (int i = 0; i < traveler_count; i++) {
        if (travelers[i].pipe_fd[0] != -1) {
            close(travelers[i].pipe_fd[0]);
        }

        if (travelers[i].pid > 0) {
            waitpid(travelers[i].pid, NULL, 0);
        }
    }

    free(info);
    free_graph(g);

    CloseWindow();

    return 0;
}