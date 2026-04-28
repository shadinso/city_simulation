#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "graph.h"

/*
 * Run Dijkstra from 'src' to 'dst' on graph g.
 * * On success:
 * - path[]  is filled with the node IDs of the shortest path
 * - path_len is set to the number of nodes in the path
 * - returns the total cost (>= 0)
 * * If no path exists: returns -1, path_len = 0
 * If src == dst   : path = {src}, path_len = 1, cost = 0
 * * Caller must free path[] with free().
 */
int dijkstra(const Graph *g, int src, int dst,
             int **path, int *path_len);

#endif /* DIJKSTRA_H */
