//
// Renderer - cleaned up version
//
// Changes vs. the original version:
//  - build_layout() replaces the old fixed "city" positions with a
//    generic, low-crossing circular layout (hub node in the center).
//  - draw_road() now draws a small rounded "badge" behind each edge
//    weight so numbers don't get lost in crossing lines, and offsets
//    edges that go in both directions so they don't overlap.
//  - draw_district() draws a soft glow ring around each node so
//    travelers are easier to spot on top of it.
//  - draw_header() adds a title bar at the top of the window
//    (previously the COL_TITLE / COL_SUBTITLE / COL_HEADER_BG colors
//    were defined but never actually used).
//
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "renderer.h"
#include "raylib.h"

#define COL_BG          CLITERAL(Color){ 30,  35,  46, 255 }
#define COL_GRID        CLITERAL(Color){ 42,  48,  60, 255 }
#define COL_ROAD        CLITERAL(Color){170, 175, 190, 255 }
#define COL_ARROW       CLITERAL(Color){235, 185,  45, 255 }
#define COL_BADGE_BG    CLITERAL(Color){ 18,  20,  30, 230 }
#define COL_BADGE_BORD  CLITERAL(Color){235, 185,  45, 255 }
#define COL_BADGE_TEXT  CLITERAL(Color){255, 220,  70, 255 }
#define COL_NODE_GLOW   CLITERAL(Color){120, 190, 255,  60 }
#define COL_NODE_RING   CLITERAL(Color){140, 200, 255, 255 }
#define COL_NODE_FILL   CLITERAL(Color){ 45, 110, 215, 255 }
#define COL_NODE_TEXT   CLITERAL(Color){255, 255, 255, 255 }
#define COL_TITLE       CLITERAL(Color){150, 205, 255, 255 }
#define COL_SUBTITLE    CLITERAL(Color){120, 145, 175, 255 }
#define COL_HEADER_BG   CLITERAL(Color){ 18,  22,  34, 255 }

/* -------------------------------------------------------------- */
/* Layout: hub node in the center, the rest evenly on a circle.   */
/* -------------------------------------------------------------- */
void build_layout(const Graph *g, NodeInfo *info) {
    int N = g->num_nodes;
    if (N <= 0) return;

    /* Reserve space for the header (top) and the legend (bottom)
     * so the circle never overlaps the text. */
    const float header_h = 70.0f;
    const float footer_h = 80.0f;

    float cx = SCREEN_WIDTH / 2.0f;
    float cy = header_h + (SCREEN_HEIGHT - header_h - footer_h) / 2.0f;

    float radius = (SCREEN_HEIGHT - header_h - footer_h) / 2.0f - 40.0f;
    float max_radius_w = SCREEN_WIDTH / 2.0f - 80.0f;
    if (radius > max_radius_w) radius = max_radius_w;
    if (radius < 60.0f) radius = 60.0f;

    if (N == 1) {
        info[0].x = cx;
        info[0].y = cy;
        return;
    }

    /* Find the node with the highest degree (in + out edges) and
     * place it at the center -> drastically reduces crossing lines
     * for "hub" style graphs (e.g. one central node feeding several
     * others). */
    int *degree = (int *)calloc(N, sizeof(int));
    for (int i = 0; i < N; i++) {
        for (Edge *e = g->nodes[i].head; e; e = e->next) {
            degree[i]++;
            degree[e->dest]++;
        }
    }

    int center_node = 0;
    for (int i = 1; i < N; i++) {
        if (degree[i] > degree[center_node]) center_node = i;
    }
    free(degree);

    info[center_node].x = cx;
    info[center_node].y = cy;

    int ring_count = N - 1;
    int placed = 0;
    for (int i = 0; i < N; i++) {
        if (i == center_node) continue;

        /* start at the top (-90 deg) and go clockwise */
        float angle = (2.0f * PI * placed) / ring_count - PI / 2.0f;
        info[i].x = cx + radius * cosf(angle);
        info[i].y = cy + radius * sinf(angle);
        placed++;
    }
}

/* -------------------------------------------------------------- */
static void draw_grid(void) {
    for (int x = 0; x < SCREEN_WIDTH; x += 50)
        DrawLine(x, 0, x, SCREEN_HEIGHT, COL_GRID);

    for (int y = 0; y < SCREEN_HEIGHT; y += 50)
        DrawLine(0, y, SCREEN_WIDTH, y, COL_GRID);
}

static void draw_arrowhead(float tx, float ty, float ux, float uy, Color c) {
    float size = 13.0f, wing = 6.0f;

    Vector2 tip   = { tx, ty };
    Vector2 left  = { tx - size*ux + wing*(-uy), ty - size*uy + wing*(ux)  };
    Vector2 right = { tx - size*ux + wing*(uy),  ty - size*uy + wing*(-ux) };

    DrawTriangle(tip, left, right, c);
}

/* Returns 1 if the graph has an edge a -> b, 0 otherwise. */
static int has_edge(const Graph *g, int a, int b) {
    for (Edge *e = g->nodes[a].head; e; e = e->next) {
        if (e->dest == b) return 1;
    }
    return 0;
}

/* -------------------------------------------------------------- */
static void draw_road(const Graph *g, int u, int v,
                       NodeInfo from, NodeInfo to, int weight) {
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 1) return;

    float ux = dx / len;
    float uy = dy / len;

    /* perpendicular unit vector, used to separate edges that go
     * in both directions between the same pair of nodes */
    float px = -uy;
    float py = ux;

    float offset = 0.0f;
    if (has_edge(g, v, u)) {
        offset = (u < v) ? 8.0f : -8.0f;
    }

    float sx = from.x + ux * (NODE_RADIUS + 4)  + px * offset;
    float sy = from.y + uy * (NODE_RADIUS + 4)  + py * offset;
    float ex = to.x   - ux * (NODE_RADIUS + 16) + px * offset;
    float ey = to.y   - uy * (NODE_RADIUS + 16) + py * offset;

    DrawLineEx((Vector2){sx, sy}, (Vector2){ex, ey}, 3, COL_ROAD);

    float tip_x = to.x - ux * (NODE_RADIUS + 2) + px * offset;
    float tip_y = to.y - uy * (NODE_RADIUS + 2) + py * offset;
    draw_arrowhead(tip_x, tip_y, ux, uy, COL_ARROW);

    /* weight badge: a small rounded rectangle with the weight
     * printed inside, pushed to the side of the road so it never
     * sits directly on top of a line */
    float bx = sx + (ex - sx) * 0.35f + px * 18.0f;
    float by = sy + (ey - sy) * 0.35f + py * 18.0f;

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", weight);
    int tw = MeasureText(buf, 14);

    Rectangle badge = { bx - tw/2.0f - 5, by - 11, (float)(tw + 10), 22 };
    DrawRectangleRounded(badge, 0.4f, 6, COL_BADGE_BG);
    DrawRectangleRoundedLines(badge, 0.4f, 6, COL_BADGE_BORD);
    DrawText(buf, (int)(bx - tw/2.0f), (int)(by - 7), 14, COL_BADGE_TEXT);
}

/* -------------------------------------------------------------- */
static void draw_district(NodeInfo info, int id) {
    /* soft glow behind the node, helps travelers stand out on top */
    DrawCircle((int)info.x, (int)info.y, NODE_RADIUS + 8, COL_NODE_GLOW);
    DrawCircle((int)info.x, (int)info.y, NODE_RADIUS, COL_NODE_FILL);
    DrawCircleLines((int)info.x, (int)info.y, NODE_RADIUS, COL_NODE_RING);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", id);
    int tw = MeasureText(buf, 22);
    DrawText(buf, (int)(info.x - tw/2.0f), (int)(info.y - 11), 22, COL_NODE_TEXT);
}

/* -------------------------------------------------------------- */
void draw_graph(const Graph *g, const NodeInfo *info) {
    draw_grid();

    for (int i = 0; i < g->num_nodes; i++) {
        for (Edge *e = g->nodes[i].head; e; e = e->next) {
            draw_road(g, i, e->dest, info[i], info[e->dest], e->weight);
        }
    }

    for (int i = 0; i < g->num_nodes; i++) {
        draw_district(info[i], i);
    }
}

/* -------------------------------------------------------------- */
void draw_header(const char *title, const char *subtitle) {
    DrawRectangle(0, 0, SCREEN_WIDTH, 60, COL_HEADER_BG);
    DrawLine(0, 60, SCREEN_WIDTH, 60, COL_NODE_RING);

    int tw = MeasureText(title, 26);
    DrawText(title, (SCREEN_WIDTH - tw) / 2, 8, 26, COL_TITLE);

    if (subtitle) {
        int sw = MeasureText(subtitle, 14);
        DrawText(subtitle, (SCREEN_WIDTH - sw) / 2, 38, 14, COL_SUBTITLE);
    }
}
