#include <stdio.h>
#include <stdlib.h>

#include "graph.h"
#include "dijkstra.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return 1;
    }

    Graph *g = NULL;
    int src, dst;

    if (read_graph_from_file(argv[1], &g, &src, &dst) != 0)
    {
        return 1;
    }

    int *path = NULL;
    int path_len = 0;

    int cost = dijkstra(
        g,
        src,
        dst,
        &path,
        &path_len
    );

    if (cost < 0)
    {
        printf("No path found\n");
        free_graph(g);
        return 0;
    }

    for (int i = 0; i < path_len; i++)
    {
        printf("%d", path[i]);

        if (i < path_len - 1)
        {
            printf(" -> ");
        }
    }

    printf("\n%d\n", cost);

    free(path);
    free_graph(g);

    return 0;
}
