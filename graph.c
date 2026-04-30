#include <stdio.h>
#include <stdlib.h>
#include "graph.h"

/* -------------------------------------------------- */
/* Create an empty graph with num_nodes nodes         */
/* -------------------------------------------------- */
Graph *create_graph(int num_nodes, int num_edges) {
    Graph *g = (Graph *)malloc(sizeof(Graph));
    if (!g) {
        fprintf(stderr, "Error: memory allocation failed for graph\n");
        return NULL;
    }

    g->num_nodes = num_nodes;
    g->num_edges = num_edges;

    g->nodes = (Node *)malloc(num_nodes * sizeof(Node));
    if (!g->nodes) {
        fprintf(stderr, "Error: memory allocation failed for nodes\n");
        free(g);
        return NULL;
    }

    /* initialise every node: id = index, no edges yet */
    for (int i = 0; i < num_nodes; i++) {
        g->nodes[i].id   = i;
        g->nodes[i].head = NULL;
    }

    return g;
}

/* -------------------------------------------------- */
/* Add a directed edge src -> dest with given weight  */
/* -------------------------------------------------- */
void add_edge(Graph *g, int src, int dest, int weight) {
    Edge *e = (Edge *)malloc(sizeof(Edge));
    if (!e) {
        fprintf(stderr, "Error: memory allocation failed for edge\n");
        return;
    }

    e->dest   = dest;
    e->weight = weight;
    e->next   = g->nodes[src].head;   /* insert at front of list */
    g->nodes[src].head = e;
}

/* -------------------------------------------------- */
/* Free all memory used by the graph                  */
/* -------------------------------------------------- */
void free_graph(Graph *g) {
    if (!g) return;

    for (int i = 0; i < g->num_nodes; i++) {
        Edge *cur = g->nodes[i].head;
        while (cur) {
            Edge *tmp = cur;
            cur = cur->next;
            free(tmp);
        }
    }

    free(g->nodes);
    free(g);
}

/* -------------------------------------------------- */
/* Read graph + query from file                       */
/* Returns 0 on success, -1 on any error              */
/* -------------------------------------------------- */
int read_graph_from_file(const char *filename,
                         Graph **out_graph,
                         int *src_query,
                         int *dst_query)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open file '%s'\n", filename);
        return -1;
    }

    int N, M;
    if (fscanf(fp, "%d %d", &N, &M) != 2 || N <= 0 || M < 0) {
        fprintf(stderr, "Error: invalid graph header in file\n");
        fclose(fp);
        return -1;
    }

    Graph *g = create_graph(N, M);
    if (!g) {
        fclose(fp);
        return -1;
    }

    /* read M edges */
    for (int i = 0; i < M; i++) {
        int src, dst, w;
        if (fscanf(fp, "%d %d %d", &src, &dst, &w) != 3) {
            fprintf(stderr, "Error: malformed edge #%d\n", i + 1);
            free_graph(g);
            fclose(fp);
            return -1;
        }

        /* validate node indices */
        if (src < 0 || src >= N || dst < 0 || dst >= N) {
            fprintf(stderr, "Error: node index out of range in edge %d->%d\n", src, dst);
            free_graph(g);
            fclose(fp);
            return -1;
        }

        /* negative weights are not allowed with Dijkstra */
        if (w < 0) {
            fprintf(stderr, "Error: negative weight (%d) is not allowed\n", w);
            free_graph(g);
            fclose(fp);
            return -1;
        }

        add_edge(g, src, dst, w);
    }

    /* read query: source and destination */
    if (fscanf(fp, "%d %d", src_query, dst_query) != 2) {
        fprintf(stderr, "Error: missing source/destination query\n");
        free_graph(g);
        fclose(fp);
        return -1;
    }

    if (*src_query < 0 || *src_query >= N ||
        *dst_query < 0 || *dst_query >= N) {
        fprintf(stderr, "Error: query node index out of range\n");
        free_graph(g);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    *out_graph = g;
    return 0;
}