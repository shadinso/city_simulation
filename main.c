/*
 * main.c - Milestone 6: Node Synchronization using POSIX Semaphores
 *
 * At any given moment, only ONE traveler may occupy a node.
 * Others wait outside (blocked on the semaphore).
 * The GUI shows waiting travelers in a different color (YELLOW).
 *
 * IPC: pipes (same as milestone 5)
 * Sync: POSIX named semaphores, one per node (/city_node_0, /city_node_1, ...)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>

#include "raylib.h"
#include "graph.h"
#include "dijkstra.h"
#include "renderer.h"

#define MAX_TRAVELERS 20
#define SEM_NAME_PREFIX "/city_node_"

#define MSG_NODE      1
#define MSG_FINISHED  2
#define MSG_WAITING   3
#define MSG_ENTERED   4

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
    int waiting;

    int current_node;
    int next_node;

    float x;
    float y;
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

static int read_input_file(const char *filename, Graph **out_graph,
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
        travelers[i].waiting = 0;
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

static void send_message(int fd, int type, int index,
                         int current_node, int next_node) {
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
        int next = (i < path_len - 1) ? path[i + 1] : -1;

        char sem_name[64];
        make_sem_name(sem_name, current);

        sem_t *node_sem = sem_open(sem_name, 0);
        if (node_sem == SEM_FAILED) {
            perror("sem_open");
            node_sem = NULL;
        } else {
            send_message(write_fd, MSG_WAITING, index, current, next);

            if (sem_wait(node_sem) == -1) {
                perror("sem_wait");
            }
        }

        send_message(write_fd, MSG_ENTERED, index, current, next);
        send_message(write_fd, MSG_NODE, index, current, next);

        if (i > 0 && i < path_len - 1) {
            sleep(1);
        }

        if (node_sem) {
            if (sem_post(node_sem) == -1) {
                perror("sem_post");
            }
            sem_close(node_sem);
        }

        if (next != -1) {
            int weight = get_edge_weight(g, current, next);
            if (weight <= 0) weight = 1;
            usleep(weight * 300000);
        }
    }

    send_message(write_fd, MSG_FINISHED, index, path[path_len - 1], -1);

    close(write_fd);
    free(path);
    exit(0);
}

static void handle_message(IPCMessage msg, Traveler travelers[], NodeInfo *info) {
    int i = msg.traveler_index;

    switch (msg.type) {
        case MSG_WAITING:
            travelers[i].waiting = 1;
            printf("[PID=%d] waiting outside node %d\n", msg.pid, msg.current_node);
            fflush(stdout);
            break;

        case MSG_ENTERED:
            travelers[i].waiting = 0;
            break;

        case MSG_NODE:
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

        case MSG_FINISHED:
            travelers[i].finished = 1;
            travelers[i].active = 0;
            travelers[i].waiting = 0;

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

    if (read_input_file(argv[1], &g, travelers, &traveler_count) != 0) {
        return 1;
    }

    NodeInfo *info = malloc(g->num_nodes * sizeof(NodeInfo));
    if (!info) {
        free_graph(g);
        return 1;
    }

    build_city_layout(g, info);

    sem_t *node_sems[MAX_NODES];

    for (int i = 0; i < g->num_nodes; i++) {
        char name[64];
        make_sem_name(name, i);
        sem_unlink(name);

        node_sems[i] = sem_open(name, O_CREAT | O_EXCL, 0666, 1);
        if (node_sems[i] == SEM_FAILED) {
            perror("sem_open (create)");
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

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "City Simulation - Milestone 6 Sync");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        for (int i = 0; i < traveler_count; i++) {
            if (travelers[i].pipe_fd[0] == -1) {
                continue;
            }

            IPCMessage msg;
            ssize_t bytes;

            while ((bytes = read(travelers[i].pipe_fd[0],
                                 &msg,
                                 sizeof(msg))) == sizeof(msg)) {
                handle_message(msg, travelers, info);
            }
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);

        draw_graph(g, info);

        for (int i = 0; i < traveler_count; i++) {
            if (travelers[i].finished) {
                continue;
            }

            Color display_color = travelers[i].waiting ? YELLOW : travelers[i].color;
            int radius = travelers[i].waiting ? 7 : 10;

            int wait_rank = 0;
            for (int j = 0; j < i; j++) {
                if (travelers[j].waiting) {
                    wait_rank++;
                }
            }

            int offset = travelers[i].waiting ? (18 + wait_rank * 14) : 0;

            DrawCircle((int)travelers[i].x,
                       (int)travelers[i].y + offset,
                       radius,
                       display_color);

            char label[16];
            sprintf(label, travelers[i].waiting ? "W%d" : "T%d", i + 1);

            DrawText(label,
                     (int)travelers[i].x + 12,
                     (int)travelers[i].y - 8 + offset,
                     14,
                     WHITE);
        }

        DrawText("T# = traveling   W# = waiting for node",
                 20,
                 SCREEN_HEIGHT - 60,
                 16,
                 RAYWHITE);

        DrawText("Milestone 6 - Node Sync (semaphores)",
                 20,
                 SCREEN_HEIGHT - 35,
                 20,
                 RAYWHITE);

        int all_done = 1;
        for (int i = 0; i < traveler_count; i++) {
            if (!travelers[i].finished) {
                all_done = 0;
                break;
            }
        }

        if (all_done) {
            DrawText("ALL FINISHED!", 20, 30, 30, GREEN);
        }

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

    for (int i = 0; i < g->num_nodes; i++) {
        char name[64];
        make_sem_name(name, i);

        if (node_sems[i]) {
            sem_close(node_sems[i]);
        }

        sem_unlink(name);
    }

    free(info);
    free_graph(g);
    CloseWindow();

    return 0;
}
