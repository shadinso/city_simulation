#include <stdio.h>
#include <stdlib.h>
#include "graph.h"
#include "dijkstra.h"

int main(int argc, char *argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return 1;
    }

    /* ---- read graph and query from file ---- */
    Graph *g = NULL;
    int src = 0, dst = 0;

    if (read_graph_from_file(argv[1], &g, &src, &dst) != 0) {
        return 1;   /* error message already printed inside */
    }

    /* ---- run Dijkstra ---- */
    int *path = NULL;
    int path_len = 0;

    int cost = dijkstra(g, src, dst, &path, &path_len);

    /* ---- print result ---- */
    if (cost == -1) {
        printf("No path found\n");
    } else {
        /* print path: 0 -> 2 -> 1 -> 3 -> 4 -> 5 */
        for (int i = 0; i < path_len; i++) {
            if (i > 0)
                printf(" -> ");
            printf("%d", path[i]);
        }
        printf("\n%d\n", cost);
    }

    /* ---- clean up ---- */
    free(path);
    free_graph(g);

    return 0;
}