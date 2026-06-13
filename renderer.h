#ifndef RENDERER_H
#define RENDERER_H

#include "graph.h"

#define SCREEN_WIDTH  1000
#define SCREEN_HEIGHT 700
#define NODE_RADIUS   32

typedef struct {
    float x;
    float y;
} NodeInfo;

/*
 * Computes node positions for a clean, low-crossing drawing:
 * the node with the highest degree (most incoming + outgoing
 * edges) is placed at the center of the screen, and all other
 * nodes are spread evenly on a circle around it.
 *
 * This works automatically for any graph size (up to MAX_NODES)
 * without needing a hand-made layout per input file.
 */
void build_layout(const Graph *g, NodeInfo *info);

/*
 * Draws the static graph: background grid, roads (directed edges
 * with arrowheads and a small "badge" showing the weight), and
 * districts (nodes drawn as glowing circles with their id).
 */
void draw_graph(const Graph *g, const NodeInfo *info);

/*
 * Draws a styled title bar at the top of the screen with a title
 * and an optional subtitle line.
 */
void draw_header(const char *title, const char *subtitle);

#endif /* RENDERER_H */
