/*
 * main.c - Milestone 6: Node Synchronization using POSIX Semaphores
 *
 * At any given moment, only ONE traveler may occupy a node.
 * Others wait outside (blocked on the semaphore).
 * The GUI shows waiting travelers in YELLOW, moving travelers in their color.
 * Smooth interpolation between nodes (no jumping/teleporting).
 *
 * IPC: pipes (one per traveler, non-blocking reads in GUI loop)
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
#define MSG_ENTERED   4

/* How many microseconds per unit of edge weight in child timing.
 * 1500000 = 1.5 seconds per weight unit -> edge weight 4 = ~6 sec.
 * Slow enough that the smooth movement along each edge is clearly
 * visible, while keeping the whole demo within ~30-60 s. */
#define USEC_PER_WEIGHT 1500000

typedef struct {
    int   type;
    pid_t pid;
    int   traveler_index;
    int   current_node;
    int   next_node;
} IPCMessage;

typedef struct {
    int   src;
    int   dst;
    pid_t pid;
    int   pipe_fd[2];

    Color color;
    int   active;
    int   finished;
    int   waiting;

    /* current drawn position (interpolated) */
    float x;
    float y;

    /* smooth movement */
    float from_x;
    float from_y;
    float to_x;
    float to_y;
    float move_progress;   /* 0.0 -> 1.0 */
    float move_duration;   /* seconds for current edge */
    int   is_moving;
    int   placed;          /* 0 until the traveler has been drawn once */

    int   current_node;
    int   next_node;
} Traveler;

/* ------------------------------------------------------------------ */
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

/* ------------------------------------------------------------------ */
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
    if (!g) { fclose(fp); return -1; }

    for (int i = 0; i < M; i++) {
        int src, dst, w;
        if (fscanf(fp, "%d %d %d", &src, &dst, &w) != 3) {
            fprintf(stderr, "Error: malformed edge\n");
            free_graph(g); fclose(fp); return -1;
        }
        if (src < 0 || src >= N || dst < 0 || dst >= N) {
            fprintf(stderr, "Error: node index out of range\n");
            free_graph(g); fclose(fp); return -1;
        }
        if (w < 0) {
            fprintf(stderr, "Error: negative weight (%d) not allowed\n", w);
            free_graph(g); fclose(fp); return -1;
        }
        add_edge(g, src, dst, w);
    }

    int count;
    if (fscanf(fp, "%d", &count) != 1 || count <= 0 || count > MAX_TRAVELERS) {
        fprintf(stderr, "Error: invalid travelers count\n");
        free_graph(g); fclose(fp); return -1;
    }

    for (int i = 0; i < count; i++) {
        int src, dst;
        if (fscanf(fp, "%d %d", &src, &dst) != 2) {
            fprintf(stderr, "Error: malformed traveler\n");
            free_graph(g); fclose(fp); return -1;
        }
        if (src < 0 || src >= N || dst < 0 || dst >= N) {
            fprintf(stderr, "Error: traveler node out of range\n");
            free_graph(g); fclose(fp); return -1;
        }

        travelers[i].src          = src;
        travelers[i].dst          = dst;
        travelers[i].pid          = -1;
        travelers[i].pipe_fd[0]   = -1;
        travelers[i].pipe_fd[1]   = -1;
        travelers[i].active       = 1;
        travelers[i].finished     = 0;
        travelers[i].waiting      = 0;
        travelers[i].current_node = src;
        travelers[i].next_node    = -1;
        travelers[i].x            = 0;
        travelers[i].y            = 0;
        travelers[i].from_x       = 0;
        travelers[i].from_y       = 0;
        travelers[i].to_x         = 0;
        travelers[i].to_y         = 0;
        travelers[i].move_progress = 1.0f;
        travelers[i].move_duration = 1.0f;
        travelers[i].is_moving    = 0;
        travelers[i].placed       = 0;
    }

    fclose(fp);
    *out_graph      = g;
    *traveler_count = count;
    return 0;
}

/* ------------------------------------------------------------------ */
static void send_message(int fd, int type, int index,
                         int current_node, int next_node) {
    IPCMessage msg;
    msg.type           = type;
    msg.pid            = getpid();
    msg.traveler_index = index;
    msg.current_node   = current_node;
    msg.next_node      = next_node;
    write(fd, &msg, sizeof(msg));
}

/* ------------------------------------------------------------------ */
/* Child: computes own Dijkstra path, walks it, reports via pipe.     */
static void child_process(Graph *g, Traveler t, int index, int write_fd) {
    int *path     = NULL;
    int  path_len = 0;

    int cost = dijkstra(g, t.src, t.dst, &path, &path_len);

    if (cost < 0 || path_len <= 0) {
        send_message(write_fd, MSG_FINISHED, index, t.src, -1);
        close(write_fd);
        free(path);
        exit(0);
    }

    for (int i = 0; i < path_len; i++) {
        int current = path[i];
        int next    = (i < path_len - 1) ? path[i + 1] : -1;

        /* --- acquire node semaphore --- */
        char  sem_name[64];
        make_sem_name(sem_name, current);

        sem_t *node_sem = sem_open(sem_name, 0);
        if (node_sem == SEM_FAILED) {
            perror("sem_open");
            node_sem = NULL;
        } else {
            /* Tell parent we are waiting BEFORE blocking */
            send_message(write_fd, MSG_WAITING, index, current, next);

            if (sem_wait(node_sem) == -1) {
                perror("sem_wait");
            }
        }

        /* Tell parent we entered the node */
        send_message(write_fd, MSG_ENTERED,  index, current, next);
        send_message(write_fd, MSG_NODE,     index, current, next);

        /* Stay 1 second at intermediate nodes (not source, not destination) */
        if (i > 0 && i < path_len - 1) {
            sleep(1);
        }

        /* Release semaphore */
        if (node_sem) {
            sem_post(node_sem);
            sem_close(node_sem);
        }

        /* Travel along the outgoing edge */
        if (next != -1) {
            int weight = get_edge_weight(g, current, next);
            if (weight <= 0) weight = 1;
            usleep(weight * USEC_PER_WEIGHT);
        }
    }

    send_message(write_fd, MSG_FINISHED, index, path[path_len - 1], -1);

    close(write_fd);
    free(path);
    exit(0);
}

/* ------------------------------------------------------------------ */
/* Parent: handles one IPC message and updates traveler state.        */
static void handle_message(IPCMessage msg, Traveler travelers[],
                            NodeInfo *info, Graph *g) {
    int i = msg.traveler_index;

    switch (msg.type) {

        case MSG_WAITING:
            travelers[i].waiting = 1;
            printf("[PID=%d] waiting outside node %d\n",
                   msg.pid, msg.current_node);
            fflush(stdout);
            break;

        case MSG_ENTERED:
            travelers[i].waiting = 0;
            break;

        case MSG_NODE: {
            int prev_node = travelers[i].current_node;

            travelers[i].current_node = msg.current_node;
            travelers[i].next_node    = msg.next_node;

            /* --- start smooth movement from previous position to new node --- */
            travelers[i].from_x = travelers[i].x;
            travelers[i].from_y = travelers[i].y;
            travelers[i].to_x   = info[msg.current_node].x;
            travelers[i].to_y   = info[msg.current_node].y;

            /*
             * Snap instantly ONLY the very first time the traveler is
             * placed on the board (it should appear right at its
             * source node, not slide in from nowhere). Every later
             * arrival must be animated, so the traveler clearly
             * slides along the edge instead of teleporting.
             *
             * NOTE: previously this also checked
             * `travelers[i].move_progress >= 1.0f`, but that flag is
             * (almost) always 1.0 by the time a new MSG_NODE arrives,
             * which made EVERY hop snap instantly. That was the cause
             * of travelers appearing to "jump"/teleport between nodes.
             */
            if (!travelers[i].placed) {
                travelers[i].x             = travelers[i].to_x;
                travelers[i].y             = travelers[i].to_y;
                travelers[i].move_progress = 1.0f;
                travelers[i].is_moving     = 0;
                travelers[i].placed        = 1;
            } else {
                int w = get_edge_weight(g, prev_node, msg.current_node);
                if (w <= 0) w = 1;
                /* duration in seconds = weight * USEC_PER_WEIGHT / 1e6 */
                travelers[i].move_duration = (float)(w * USEC_PER_WEIGHT) / 1000000.0f;
                travelers[i].move_progress = 0.0f;
                travelers[i].is_moving     = 1;
            }

            if (msg.next_node == -1) {
                printf("[PID=%d] arrived at node %d | DESTINATION\n",
                       msg.pid, msg.current_node);
            } else {
                printf("[PID=%d] arrived at node %d | next node: %d\n",
                       msg.pid, msg.current_node, msg.next_node);
            }
            fflush(stdout);
            break;
        }

        case MSG_FINISHED:
            travelers[i].finished = 1;
            travelers[i].active   = 0;
            travelers[i].waiting  = 0;
            travelers[i].is_moving = 0;
            printf("[PID=%d] finished\n", msg.pid);
            fflush(stdout);
            break;
    }
}

/* ------------------------------------------------------------------ */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return 1;
    }

    Graph   *g = NULL;
    Traveler travelers[MAX_TRAVELERS];
    int      traveler_count = 0;

    if (read_input_file(argv[1], &g, travelers, &traveler_count) != 0)
        return 1;

    NodeInfo *info = malloc(g->num_nodes * sizeof(NodeInfo));
    if (!info) { free_graph(g); return 1; }

    build_layout(g, info);

    /* --- Create one named semaphore per node (value = 1) --- */
    sem_t *node_sems[MAX_NODES];
    for (int i = 0; i < g->num_nodes; i++) {
        char name[64];
        make_sem_name(name, i);
        sem_unlink(name);   /* clean up any leftover from a previous run */

        node_sems[i] = sem_open(name, O_CREAT | O_EXCL, 0666, 1);
        if (node_sems[i] == SEM_FAILED) {
            perror("sem_open (create)");
            node_sems[i] = NULL;
        }
    }

    /* Distinct colors for up to MAX_TRAVELERS travelers */
    Color colors[MAX_TRAVELERS] = {
        RED, BLUE, GREEN, ORANGE, PURPLE,
        YELLOW, PINK, SKYBLUE, LIME, MAROON,
        GOLD, VIOLET, BROWN, BEIGE, MAGENTA,
        DARKGREEN, DARKBLUE, DARKPURPLE, DARKBROWN, RAYWHITE
    };

    /* --- Fork one child per traveler --- */
    for (int i = 0; i < traveler_count; i++) {
        travelers[i].color = colors[i % MAX_TRAVELERS];

        /* Start position = source node */
        travelers[i].x      = info[travelers[i].src].x;
        travelers[i].y      = info[travelers[i].src].y;
        travelers[i].from_x = travelers[i].x;
        travelers[i].from_y = travelers[i].y;
        travelers[i].to_x   = travelers[i].x;
        travelers[i].to_y   = travelers[i].y;

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
            /* Child: close read end */
            close(travelers[i].pipe_fd[0]);
            child_process(g, travelers[i], i, travelers[i].pipe_fd[1]);
            /* child_process calls exit() -- never returns */

        } else {
            /* Parent: close write end, set pipe non-blocking */
            travelers[i].pid = pid;
            close(travelers[i].pipe_fd[1]);

            int flags = fcntl(travelers[i].pipe_fd[0], F_GETFL, 0);
            fcntl(travelers[i].pipe_fd[0], F_SETFL, flags | O_NONBLOCK);
        }
    }

    /* ============================  GUI LOOP  ============================ */
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "City Simulation - Milestone 6 Sync");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {

        float dt = GetFrameTime();

        /* --- Read all pending IPC messages from every pipe --- */
        for (int i = 0; i < traveler_count; i++) {
            if (travelers[i].pipe_fd[0] == -1) continue;

            IPCMessage msg;
            ssize_t    bytes;

            while ((bytes = read(travelers[i].pipe_fd[0],
                                 &msg,
                                 sizeof(msg))) == sizeof(msg)) {
                handle_message(msg, travelers, info, g);
            }
        }

        /* --- Update smooth movement for every traveler --- */
        for (int i = 0; i < traveler_count; i++) {
            if (!travelers[i].is_moving) continue;

            travelers[i].move_progress += dt / travelers[i].move_duration;

            if (travelers[i].move_progress >= 1.0f) {
                travelers[i].move_progress = 1.0f;
                travelers[i].is_moving     = 0;
                travelers[i].x             = travelers[i].to_x;
                travelers[i].y             = travelers[i].to_y;
            } else {
                /* Linear interpolation */
                float t = travelers[i].move_progress;
                travelers[i].x = travelers[i].from_x +
                                 (travelers[i].to_x - travelers[i].from_x) * t;
                travelers[i].y = travelers[i].from_y +
                                 (travelers[i].to_y - travelers[i].from_y) * t;
            }
        }

        /* --- Draw --- */
        BeginDrawing();
        ClearBackground(DARKGRAY);

        draw_graph(g, info);

        /* Draw each traveler */
        for (int i = 0; i < traveler_count; i++) {
            if (travelers[i].finished) continue;

            Color display_color = travelers[i].waiting
                                      ? YELLOW
                                      : travelers[i].color;
            int radius = travelers[i].waiting ? 9 : 13;

            /* Stack waiting travelers so they don't overlap completely */
            int offset_y = 0;
            if (travelers[i].waiting) {
                int rank = 0;
                for (int j = 0; j < i; j++) {
                    if (travelers[j].waiting &&
                        travelers[j].current_node == travelers[i].current_node) {
                        rank++;
                    }
                }
                offset_y = (rank + 1) * 26;   /* push each waiter down */
            }

            DrawCircle((int)travelers[i].x,
                       (int)travelers[i].y + offset_y,
                       radius,
                       display_color);

            DrawCircleLines((int)travelers[i].x,
                             (int)travelers[i].y + offset_y,
                             radius,
                             BLACK);

            char label[16];
            sprintf(label, travelers[i].waiting ? "W%d" : "T%d", i + 1);

            DrawText(label,
                     (int)travelers[i].x + 15,
                     (int)travelers[i].y - 8 + offset_y,
                     16,
                     WHITE);
        }

        /* Header / title bar */
        draw_header("City Simulation - Graph Traffic",
                     "Milestone 6 - Node Synchronization (semaphores) | IPC: pipes");

        /* Legend */
        DrawText("T# = traveling     W# = waiting outside a full node",
                 20, SCREEN_HEIGHT - 60, 16, RAYWHITE);
        DrawText("Each traveler stays exactly 1 second inside a node it enters",
                 20, SCREEN_HEIGHT - 35, 16, RAYWHITE);

        /* All-done banner */
        int all_done = 1;
        for (int i = 0; i < traveler_count; i++) {
            if (!travelers[i].finished) { all_done = 0; break; }
        }
        if (all_done) {
            DrawText("ALL FINISHED!", 20, 75, 30, GREEN);
        }

        EndDrawing();
    }
    /* ================================================================== */

    /* Clean up */
    for (int i = 0; i < traveler_count; i++) {
        if (travelers[i].pipe_fd[0] != -1)
            close(travelers[i].pipe_fd[0]);

        if (travelers[i].pid > 0)
            waitpid(travelers[i].pid, NULL, 0);
    }

    for (int i = 0; i < g->num_nodes; i++) {
        char name[64];
        make_sem_name(name, i);
        if (node_sems[i]) sem_close(node_sems[i]);
        sem_unlink(name);
    }

    free(info);
    free_graph(g);
    CloseWindow();

    return 0;
}
