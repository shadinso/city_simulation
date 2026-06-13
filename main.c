/*
 * main.c - Milestone 6 fixed and smoother animation
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <semaphore.h>
#include <math.h>

#include "raylib.h"
#include "graph.h"
#include "dijkstra.h"
#include "renderer.h"

#define MAX_TRAVELERS 20
#define SEM_NAME_PREFIX "/city_node_"

#define MSG_NODE      1
#define MSG_FINISHED  2
#define MSG_WAITING   3
#define MSG_TRAVELING 4

#define USEC_PER_WEIGHT 5000000
#define NODE_WAIT_SECONDS 1
#define OUTSIDE_NODE 45.0f
#define VISUAL_LEAVE_DELAY 900000

typedef struct {
    int type;
    pid_t pid;
    int traveler_index;
    int current_node;
    int next_node;
} IPCMessage;

typedef struct {
    int src, dst;
    pid_t pid;
    int pipe_fd[2];

    Color color;
    int finished;
    int waiting;
    int is_moving;

    float x, y;
    float from_x, from_y;
    float to_x, to_y;
    float move_progress;
    float move_duration;

    int current_node;
    int next_node;
    int waiting_node;
} Traveler;

static void make_sem_name(char *buf, int node_id) {
    sprintf(buf, "%s%d", SEM_NAME_PREFIX, node_id);
}

static int get_edge_weight(Graph *g, int from, int to) {
    Edge *e = g->nodes[from].head;
    while (e) {
        if (e->dest == to) return e->weight;
        e = e->next;
    }
    return 1;
}

static void send_message(int fd, int type, int index, int current, int next) {
    IPCMessage msg;
    msg.type = type;
    msg.pid = getpid();
    msg.traveler_index = index;
    msg.current_node = current;
    msg.next_node = next;
    write(fd, &msg, sizeof(msg));
}

static int read_input_file(const char *filename, Graph **out_graph,
                           Traveler travelers[], int *traveler_count) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return -1;

    int N, M;
    fscanf(fp, "%d %d", &N, &M);

    Graph *g = create_graph(N, M);

    for (int i = 0; i < M; i++) {
        int s, d, w;
        fscanf(fp, "%d %d %d", &s, &d, &w);
        add_edge(g, s, d, w);
    }

    int count;
    fscanf(fp, "%d", &count);

    for (int i = 0; i < count; i++) {
        fscanf(fp, "%d %d", &travelers[i].src, &travelers[i].dst);

        travelers[i].pid = -1;
        travelers[i].pipe_fd[0] = -1;
        travelers[i].pipe_fd[1] = -1;
        travelers[i].finished = 0;
        travelers[i].waiting = 0;
        travelers[i].is_moving = 0;

        travelers[i].x = 0;
        travelers[i].y = 0;
        travelers[i].from_x = 0;
        travelers[i].from_y = 0;
        travelers[i].to_x = 0;
        travelers[i].to_y = 0;
        travelers[i].move_progress = 1.0f;
        travelers[i].move_duration = 1.0f;

        travelers[i].current_node = travelers[i].src;
        travelers[i].next_node = -1;
        travelers[i].waiting_node = -1;
    }

    fclose(fp);
    *out_graph = g;
    *traveler_count = count;
    return 0;
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
        int next = (i < path_len - 1) ? path[i + 1] : -1;

        send_message(write_fd, MSG_WAITING, index, current, next);

        char sem_name[64];
        make_sem_name(sem_name, current);

        sem_t *sem = sem_open(sem_name, 0);
        if (sem == SEM_FAILED) {
            perror("sem_open");
            sem = NULL;
        } else {
            sem_wait(sem);
        }

        send_message(write_fd, MSG_NODE, index, current, next);

        sleep(NODE_WAIT_SECONDS);

        if (next != -1) {
            send_message(write_fd, MSG_TRAVELING, index, current, next);

            /*
             * Do not release the node until the traveler visually leaves it.
             */
            usleep(VISUAL_LEAVE_DELAY);
        }

        if (sem) {
            sem_post(sem);
            sem_close(sem);
        }

        if (next != -1) {
            int w = get_edge_weight(g, current, next);
            if (w <= 0) w = 1;
            usleep(w * USEC_PER_WEIGHT);
        }
    }

    send_message(write_fd, MSG_FINISHED, index, path[path_len - 1], -1);

    close(write_fd);
    free(path);
    exit(0);
}

static void handle_message(IPCMessage msg, Traveler travelers[],
                           NodeInfo *info, Graph *g) {
    int i = msg.traveler_index;

    switch (msg.type) {
        case MSG_WAITING:
            travelers[i].waiting = 1;
            travelers[i].waiting_node = msg.current_node;
            travelers[i].next_node = msg.next_node;
            travelers[i].is_moving = 0;

            if (travelers[i].x == 0 && travelers[i].y == 0) {
                travelers[i].x = info[msg.current_node].x + OUTSIDE_NODE;
                travelers[i].y = info[msg.current_node].y;
            }

            printf("[PID=%d] waiting outside node %d\n",
                   msg.pid, msg.current_node);
            fflush(stdout);
            break;

        case MSG_NODE:
            travelers[i].waiting = 0;
            travelers[i].waiting_node = -1;
            travelers[i].is_moving = 0;

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
            break;

        case MSG_TRAVELING: {
            travelers[i].waiting = 0;
            travelers[i].waiting_node = -1;
            travelers[i].current_node = msg.current_node;
            travelers[i].next_node = msg.next_node;

            float x1 = info[msg.current_node].x;
            float y1 = info[msg.current_node].y;
            float x2 = info[msg.next_node].x;
            float y2 = info[msg.next_node].y;

            float dx = x2 - x1;
            float dy = y2 - y1;
            float len = sqrtf(dx * dx + dy * dy);
            if (len == 0) len = 1;

            float ux = dx / len;
            float uy = dy / len;

            travelers[i].from_x = x1 + ux * OUTSIDE_NODE;
            travelers[i].from_y = y1 + uy * OUTSIDE_NODE;

            travelers[i].to_x = x2 - ux * OUTSIDE_NODE;
            travelers[i].to_y = y2 - uy * OUTSIDE_NODE;

            travelers[i].x = travelers[i].from_x;
            travelers[i].y = travelers[i].from_y;

            int w = get_edge_weight(g, msg.current_node, msg.next_node);
            if (w <= 0) w = 1;

            travelers[i].move_duration =
                (float)(w * USEC_PER_WEIGHT) / 1000000.0f;

            travelers[i].move_progress = 0.0f;
            travelers[i].is_moving = 1;
            break;
        }

        case MSG_FINISHED:
            travelers[i].finished = 1;
            travelers[i].waiting = 0;
            travelers[i].is_moving = 0;

            printf("[PID=%d] finished\n", msg.pid);
            fflush(stdout);
            break;
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

    if (read_input_file(argv[1], &g, travelers, &traveler_count) != 0)
        return 1;

    NodeInfo *info = malloc(g->num_nodes * sizeof(NodeInfo));
    if (!info) {
        free_graph(g);
        return 1;
    }

    build_layout(g, info);

    sem_t *node_sems[MAX_NODES];

    for (int i = 0; i < g->num_nodes; i++) {
        char name[64];
        make_sem_name(name, i);
        sem_unlink(name);

        node_sems[i] = sem_open(name, O_CREAT | O_EXCL, 0666, 1);
        if (node_sems[i] == SEM_FAILED) {
            perror("sem_open create");
            node_sems[i] = NULL;
        }
    }

    Color colors[MAX_TRAVELERS] = {
        RED, BLUE, GREEN, ORANGE, PURPLE,
        YELLOW, PINK, SKYBLUE, LIME, MAROON,
        GOLD, VIOLET, BROWN, BEIGE, MAGENTA,
        DARKGREEN, DARKBLUE, DARKPURPLE, DARKBROWN, RAYWHITE
    };

    for (int i = 0; i < traveler_count; i++) {
        travelers[i].color = colors[i % MAX_TRAVELERS];

        travelers[i].x = info[travelers[i].src].x + OUTSIDE_NODE;
        travelers[i].y = info[travelers[i].src].y;

        if (pipe(travelers[i].pipe_fd) == -1) {
            perror("pipe");
            continue;
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
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

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "City Simulation - Milestone 6 Sync");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        for (int i = 0; i < traveler_count; i++) {
            if (travelers[i].pipe_fd[0] == -1) continue;

            IPCMessage msg;
            while (read(travelers[i].pipe_fd[0], &msg, sizeof(msg)) == sizeof(msg)) {
                handle_message(msg, travelers, info, g);
            }
        }

        for (int i = 0; i < traveler_count; i++) {
            if (!travelers[i].is_moving) continue;

            travelers[i].move_progress += dt / travelers[i].move_duration;

            if (travelers[i].move_progress >= 1.0f) {
                travelers[i].move_progress = 1.0f;
                travelers[i].is_moving = 0;
                travelers[i].x = travelers[i].to_x;
                travelers[i].y = travelers[i].to_y;
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

        BeginDrawing();
        ClearBackground(DARKGRAY);

        draw_graph(g, info);

        for (int i = 0; i < traveler_count; i++) {
            if (travelers[i].finished) continue;

            int offset_y = 0;
            if (travelers[i].waiting) {
                int rank = 0;
                for (int j = 0; j < i; j++) {
                    if (travelers[j].waiting &&
                        travelers[j].waiting_node == travelers[i].waiting_node) {
                        rank++;
                    }
                }
                offset_y = rank * 26;
            }

            DrawCircle((int)travelers[i].x,
                       (int)travelers[i].y + offset_y,
                       13,
                       travelers[i].color);

            DrawCircleLines((int)travelers[i].x,
                            (int)travelers[i].y + offset_y,
                            13,
                            BLACK);

            char label[16];
            sprintf(label, "T%d", i + 1);

            DrawText(label,
                     (int)travelers[i].x + 15,
                     (int)travelers[i].y - 8 + offset_y,
                     16,
                     WHITE);
        }

        draw_header("City Simulation - Graph Traffic",
                    "Milestone 6 - Smooth Node Synchronization");

        DrawText("Each traveler stays 1 second inside node. Others wait outside.",
                 20, SCREEN_HEIGHT - 60, 16, RAYWHITE);

        EndDrawing();
    }

    for (int i = 0; i < traveler_count; i++) {
        if (travelers[i].pipe_fd[0] != -1)
            close(travelers[i].pipe_fd[0]);

        if (travelers[i].pid > 0)
            waitpid(travelers[i].pid, NULL, 0);
    }

    for (int i = 0; i < g->num_nodes; i++) {
        char name[64];
        make_sem_name(name, i);

        if (node_sems[i])
            sem_close(node_sems[i]);

        sem_unlink(name);
    }

    free(info);
    free_graph(g);
    CloseWindow();

    return 0;
}
