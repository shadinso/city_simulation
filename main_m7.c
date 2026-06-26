/*
 * main_m7.c - Milestone 7: Scheduling Algorithms (FCFS and SJF)
 *
 * Run:
 *   ./sim -schd fcfs <file>
 *   ./sim -schd sjf  <file>
 *
 * Changes from M6:
 *   - Children no longer call sem_wait directly.
 *   - Instead, each child sends MSG_WAITING (with remaining path length
 *     as "burst") and blocks on its own ACK pipe waiting for the parent
 *     to grant entry.
 *   - The parent maintains a per-node wait queue and, when a node
 *     becomes free, picks the next traveler according to the chosen
 *     scheduling algorithm (FCFS or SJF) and sends an ACK.
 *   - A POSIX named semaphore still guards each node so the parent
 *     never grants entry while the node is occupied.
 *   - The GUI shows which scheduling algorithm is active.
 */

#define _DEFAULT_SOURCE
/*
 * main_m7.c - Milestone 7: Scheduling Algorithms (FCFS, SJF, Priority)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <semaphore.h>
#include <math.h>

#include "raylib.h"
#include "graph.h"
#include "dijkstra.h"
#include "renderer.h"

#define MAX_TRAVELERS   20
#define SEM_NAME_PREFIX "/city_m7_node_"

#define MSG_NODE      1
#define MSG_FINISHED  2
#define MSG_WAITING   3
#define MSG_TRAVELING 4

#define USEC_PER_WEIGHT  5000000
#define NODE_WAIT_SECONDS 1
#define OUTSIDE_NODE     45.0f
#define VISUAL_LEAVE_DELAY 900000

#define SCHED_FCFS     0
#define SCHED_SJF      1
#define SCHED_PRIORITY 2

typedef struct {
    int   type;
    pid_t pid;
    int   traveler_index;
    int   current_node;
    int   next_node;
    int   remaining;
} IPCMessage;

typedef struct {
    int traveler_index;
    pid_t pid;
    int remaining;
    long arrival_seq;
} WaitEntry;

typedef struct {
    WaitEntry entries[MAX_TRAVELERS];
    int count;
} NodeQueue;

typedef struct {
    int src, dst;
    pid_t pid;
    int pipe_fd[2];
    int ack_fd[2];

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

static int get_edge_weight(Graph *g, int from, int to) {
    Edge *e = g->nodes[from].head;
    while (e) {
        if (e->dest == to) return e->weight;
        e = e->next;
    }
    return 1;
}

static void send_message(int fd, int type, int index,
                         int current, int next, int remaining) {
    IPCMessage msg;
    msg.type = type;
    msg.pid = getpid();
    msg.traveler_index = index;
    msg.current_node = current;
    msg.next_node = next;
    msg.remaining = remaining;
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
    if (count > MAX_TRAVELERS) count = MAX_TRAVELERS;

    for (int i = 0; i < count; i++) {
        fscanf(fp, "%d %d", &travelers[i].src, &travelers[i].dst);
        travelers[i].pid = -1;
        travelers[i].pipe_fd[0] = -1;
        travelers[i].pipe_fd[1] = -1;
        travelers[i].ack_fd[0] = -1;
        travelers[i].ack_fd[1] = -1;
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

static void child_process(Graph *g, Traveler t, int index,
                          int write_fd, int ack_read_fd) {
    int *path = NULL;
    int path_len = 0;

    int cost = dijkstra(g, t.src, t.dst, &path, &path_len);

    if (cost < 0 || path_len <= 0) {
        send_message(write_fd, MSG_FINISHED, index, t.src, -1, 0);
        close(write_fd);
        close(ack_read_fd);
        free(path);
        exit(0);
    }

    for (int i = 0; i < path_len; i++) {
        int current = path[i];
        int next = (i < path_len - 1) ? path[i + 1] : -1;
        int remaining = path_len - 1 - i;

        send_message(write_fd, MSG_WAITING, index, current, next, remaining);

        char ack;
        read(ack_read_fd, &ack, 1);

        send_message(write_fd, MSG_NODE, index, current, next, remaining);
        sleep(NODE_WAIT_SECONDS);

        if (next != -1) {
            send_message(write_fd, MSG_TRAVELING, index, current, next, remaining);
            usleep(VISUAL_LEAVE_DELAY);
        }

        IPCMessage rel_msg;
        rel_msg.type = 99;
        rel_msg.pid = getpid();
        rel_msg.traveler_index = index;
        rel_msg.current_node = current;
        rel_msg.next_node = next;
        rel_msg.remaining = remaining;
        write(write_fd, &rel_msg, sizeof(rel_msg));

        if (next != -1) {
            int w = get_edge_weight(g, current, next);
            if (w <= 0) w = 1;
            usleep(w * USEC_PER_WEIGHT);
        }
    }

    send_message(write_fd, MSG_FINISHED, index, path[path_len - 1], -1, 0);

    close(write_fd);
    close(ack_read_fd);
    free(path);
    exit(0);
}

/* Add traveler to queue */
static void queue_add(NodeQueue *q, int traveler_index,
                      pid_t pid, int remaining, long seq) {
    if (q->count >= MAX_TRAVELERS) return;

    q->entries[q->count].traveler_index = traveler_index;
    q->entries[q->count].pid = pid;
    q->entries[q->count].remaining = remaining;
    q->entries[q->count].arrival_seq = seq;
    q->count++;
}

static void queue_remove(NodeQueue *q, int k) {
    for (int i = k; i < q->count - 1; i++)
        q->entries[i] = q->entries[i + 1];
    q->count--;
}

static int queue_pick(NodeQueue *q, int sched) {
    if (q->count == 0) return -1;

    int best = 0;

    if (sched == SCHED_FCFS) {
        for (int i = 1; i < q->count; i++) {
            if (q->entries[i].arrival_seq < q->entries[best].arrival_seq)
                best = i;
        }
        return best;
    }

    if (sched == SCHED_PRIORITY) {
        for (int i = 1; i < q->count; i++) {
            if (q->entries[i].pid < q->entries[best].pid)
                best = i;
            else if (q->entries[i].pid == q->entries[best].pid &&
                     q->entries[i].arrival_seq < q->entries[best].arrival_seq)
                best = i;
        }
        return best;
    }

    /* SJF */
    for (int i = 1; i < q->count; i++) {
        if (q->entries[i].remaining < q->entries[best].remaining)
            best = i;
        else if (q->entries[i].remaining == q->entries[best].remaining &&
                 q->entries[i].arrival_seq < q->entries[best].arrival_seq)
            best = i;
    }

    return best;
}

static void handle_message(IPCMessage msg, Traveler travelers[],
                           NodeInfo *info, Graph *g,
                           NodeQueue node_queues[],
                           int node_occupied[],
                           long *seq_counter,
                           int sched) {
    int i = msg.traveler_index;

    switch (msg.type) {

    case MSG_WAITING: {
        int node = msg.current_node;

        travelers[i].waiting = 1;
        travelers[i].waiting_node = node;
        travelers[i].next_node = msg.next_node;
        travelers[i].is_moving = 0;

        if (travelers[i].x == 0 && travelers[i].y == 0) {
            travelers[i].x = info[node].x + OUTSIDE_NODE;
            travelers[i].y = info[node].y;
        }

        printf("[PID=%d] waiting outside node %d\n", msg.pid, node);
        fflush(stdout);

        queue_add(&node_queues[node], i, msg.pid, msg.remaining, (*seq_counter)++);

        if (!node_occupied[node]) {
            int pick_k = queue_pick(&node_queues[node], sched);
            if (pick_k >= 0) {
                int winner = node_queues[node].entries[pick_k].traveler_index;
                queue_remove(&node_queues[node], pick_k);
                node_occupied[node] = 1;

                char ack = 'G';
                write(travelers[winner].ack_fd[1], &ack, 1);
            }
        }
        break;
    }

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

        travelers[i].move_duration = (float)(w * USEC_PER_WEIGHT) / 1000000.0f;
        travelers[i].move_progress = 0.0f;
        travelers[i].is_moving = 1;
        break;
    }

    case 99: {
        int node = msg.current_node;
        node_occupied[node] = 0;

        int pick_k = queue_pick(&node_queues[node], sched);
        if (pick_k >= 0) {
            int winner = node_queues[node].entries[pick_k].traveler_index;
            queue_remove(&node_queues[node], pick_k);
            node_occupied[node] = 1;

            char ack = 'G';
            write(travelers[winner].ack_fd[1], &ack, 1);
        }
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
    int sched = SCHED_FCFS;
    const char *sched_name = "FCFS";
    const char *filename = NULL;

    if (argc < 4) {
        fprintf(stderr,
                "Usage: %s -schd fcfs|sjf|priority <graph_file>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-schd") != 0) {
        fprintf(stderr, "Expected -schd flag\n");
        return 1;
    }

    if (strcmp(argv[2], "sjf") == 0) {
        sched = SCHED_SJF;
        sched_name = "SJF";
    } else if (strcmp(argv[2], "fcfs") == 0) {
        sched = SCHED_FCFS;
        sched_name = "FCFS";
    } else if (strcmp(argv[2], "priority") == 0) {
        sched = SCHED_PRIORITY;
        sched_name = "PRIORITY";
    } else {
        fprintf(stderr, "Unknown scheduling algorithm: %s\n", argv[2]);
        fprintf(stderr,
                "Usage: %s -schd fcfs|sjf|priority <graph_file>\n", argv[0]);
        return 1;
    }

    filename = argv[3];

    Graph *g = NULL;
    Traveler travelers[MAX_TRAVELERS];
    int traveler_count = 0;

    if (read_input_file(filename, &g, travelers, &traveler_count) != 0)
        return 1;

    NodeInfo *info = malloc(g->num_nodes * sizeof(NodeInfo));
    if (!info) {
        free_graph(g);
        return 1;
    }

    build_layout(g, info);

    NodeQueue *node_queues = calloc(g->num_nodes, sizeof(NodeQueue));
    int *node_occupied = calloc(g->num_nodes, sizeof(int));

    if (!node_queues || !node_occupied) {
        free(node_queues);
        free(node_occupied);
        free(info);
        free_graph(g);
        return 1;
    }

    for (int i = 0; i < g->num_nodes; i++) {
        node_queues[i].count = 0;
        node_occupied[i] = 0;
    }

    long seq_counter = 0;

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

        if (pipe(travelers[i].pipe_fd) == -1 ||
            pipe(travelers[i].ack_fd) == -1) {
            perror("pipe");
            continue;
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            close(travelers[i].pipe_fd[0]);
            close(travelers[i].pipe_fd[1]);
            close(travelers[i].ack_fd[0]);
            close(travelers[i].ack_fd[1]);
        } else if (pid == 0) {
            close(travelers[i].pipe_fd[0]);
            close(travelers[i].ack_fd[1]);

            child_process(g, travelers[i], i,
                          travelers[i].pipe_fd[1],
                          travelers[i].ack_fd[0]);
        } else {
            travelers[i].pid = pid;

            close(travelers[i].pipe_fd[1]);
            close(travelers[i].ack_fd[0]);

            int fl = fcntl(travelers[i].pipe_fd[0], F_GETFL, 0);
            fcntl(travelers[i].pipe_fd[0], F_SETFL, fl | O_NONBLOCK);
        }
    }

    char title[64];
    snprintf(title, sizeof(title),
             "City Simulation - Milestone 7 | Scheduler: %s", sched_name);

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, title);
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        for (int i = 0; i < traveler_count; i++) {
            if (travelers[i].pipe_fd[0] == -1) continue;

            IPCMessage msg;
            while (read(travelers[i].pipe_fd[0], &msg, sizeof(msg)) == sizeof(msg)) {
                handle_message(msg, travelers, info, g,
                               node_queues, node_occupied,
                               &seq_counter, sched);
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
                travelers[i].x = travelers[i].from_x +
                                 (travelers[i].to_x - travelers[i].from_x) * t;
                travelers[i].y = travelers[i].from_y +
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
                        travelers[j].waiting_node == travelers[i].waiting_node)
                        rank++;
                }
                offset_y = rank * 26;
            }

            DrawCircle((int)travelers[i].x,
                       (int)travelers[i].y + offset_y,
                       13, travelers[i].color);

            DrawCircleLines((int)travelers[i].x,
                            (int)travelers[i].y + offset_y,
                            13, BLACK);

            char label[16];
            sprintf(label, "T%d", i + 1);

            DrawText(label,
                     (int)travelers[i].x + 15,
                     (int)travelers[i].y - 8 + offset_y,
                     16, WHITE);
        }

        char subtitle[64];
        snprintf(subtitle, sizeof(subtitle),
                 "Milestone 7 - Scheduling: %s", sched_name);

        draw_header("City Simulation - Graph Traffic", subtitle);

        DrawText("Travelers wait outside node. Scheduler controls entry order.",
                 20, SCREEN_HEIGHT - 60, 16, RAYWHITE);

        char schd_label[32];
        snprintf(schd_label, sizeof(schd_label), "SCHED: %s", sched_name);

        int lw = MeasureText(schd_label, 20);

        DrawRectangle(SCREEN_WIDTH - lw - 20, SCREEN_HEIGHT - 50,
                      lw + 16, 30, DARKBLUE);

        DrawText(schd_label,
                 SCREEN_WIDTH - lw - 12,
                 SCREEN_HEIGHT - 44, 20, GOLD);

        EndDrawing();
    }

    for (int i = 0; i < traveler_count; i++) {
        if (travelers[i].pipe_fd[0] != -1)
            close(travelers[i].pipe_fd[0]);

        if (travelers[i].ack_fd[1] != -1)
            close(travelers[i].ack_fd[1]);

        if (travelers[i].pid > 0)
            waitpid(travelers[i].pid, NULL, 0);
    }

    free(node_queues);
    free(node_occupied);
    free(info);
    free_graph(g);
    CloseWindow();

    return 0;
}
