#ifndef GRAPH_H
#define GRAPH_H

#define MAX_NODES 100
#define INF 999999999

/* —– Edge in adjacency list —– */
typedef struct Edge {
    int dest;
    int weight;
    struct Edge *next;
} Edge;

/* —– Single node —– */
typedef struct {
    int id;
    Edge *head;   /* linked list of outgoing edges */
} Node;

/* —– The graph —– */
typedef struct {
    int num_nodes;
    int num_edges;
    Node *nodes;  /* array of nodes, size = num_nodes */
} Graph;

/* —– Function declarations —– */
Graph *create_graph(int num_nodes, int num_edges);
void   add_edge(Graph *g, int src, int dest, int weight);
void   free_graph(Graph *g);

/* returns 0 on success, -1 on error */
int    read_graph_from_file(const char *filename,
Graph **out_graph,
int *src_query,
int *dst_query);

#endif /* GRAPH_H */