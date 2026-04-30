#ifndef RENDERER_H
#define RENDERER_H

#include "graph.h"

#define SCREEN_WIDTH  960
#define SCREEN_HEIGHT 620
#define NODE_RADIUS   30

typedef struct {
    float       x;
    float       y;
    const char *name;
} NodeInfo;

void build_city_layout(const Graph *g, NodeInfo *info);
void draw_graph(const Graph *g, const NodeInfo *info);

#endif
