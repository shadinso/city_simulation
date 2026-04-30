//
// Created by Alaa on 4/30/26.
//
#include <math.h>
#include <stdio.h>
#include "renderer.h"
#include "raylib.h"

#define COL_BG          CLITERAL(Color){ 34,  40,  49, 255 }
#define COL_GRID        CLITERAL(Color){ 44,  52,  64, 255 }
#define COL_ROAD        CLITERAL(Color){176, 176, 190, 255 }
#define COL_ROAD_SHADOW CLITERAL(Color){ 15,  15,  20, 255 }
#define COL_ARROW       CLITERAL(Color){230, 180,  40, 255 }
#define COL_BADGE_BG    CLITERAL(Color){ 12,  14,  22, 210 }
#define COL_BADGE_BORD  CLITERAL(Color){230, 180,  40, 255 }
#define COL_BADGE_TEXT  CLITERAL(Color){255, 215,  60, 255 }
#define COL_NODE_GLOW   CLITERAL(Color){130, 190, 255,  45 }
#define COL_NODE_RING   CLITERAL(Color){130, 190, 255, 255 }
#define COL_NODE_FILL   CLITERAL(Color){ 50, 115, 220, 255 }
#define COL_NODE_TEXT   CLITERAL(Color){255, 255, 255, 255 }
#define COL_NAME_TEXT   CLITERAL(Color){180, 205, 235, 255 }
#define COL_TITLE       CLITERAL(Color){130, 190, 255, 255 }
#define COL_SUBTITLE    CLITERAL(Color){120, 140, 170, 255 }
#define COL_LEGEND      CLITERAL(Color){ 90, 105, 125, 255 }
#define COL_HEADER_BG   CLITERAL(Color){ 20,  25,  38, 255 }

void build_city_layout(const Graph *g, NodeInfo *info) {
    static const struct { float x; float y; const char *name; } layout[] = {
        { 480,  80, "Airport" },
        { 180, 195, "University" },
        { 480, 210, "Downtown" },
        { 760, 195, "Mall" },
        { 285, 365, "Hospital" },
        { 665, 365, "Harbor" },
        { 480, 490, "Station" },
    };

    int max = sizeof(layout) / sizeof(layout[0]);
    for (int i = 0; i < g->num_nodes && i < max; i++) {
        info[i].x = layout[i].x;
        info[i].y = layout[i].y;
        info[i].name = layout[i].name;
    }
}

static void draw_grid() {
    for (int x = 0; x < SCREEN_WIDTH; x += 50)
        DrawLine(x, 0, x, SCREEN_HEIGHT, COL_GRID);

    for (int y = 0; y < SCREEN_HEIGHT; y += 50)
        DrawLine(0, y, SCREEN_WIDTH, y, COL_GRID);
}

static void draw_arrowhead(float tx, float ty, float ux, float uy, Color c) {
    float size = 13.0f, wing = 6.0f;

    Vector2 tip = { tx, ty };
    Vector2 left = { tx - size*ux + wing*(-uy), ty - size*uy + wing*(ux) };
    Vector2 right = { tx - size*ux + wing*(uy), ty - size*uy + wing*(-ux) };

    DrawTriangle(tip, left, right, c);
}

static void draw_road(NodeInfo from, NodeInfo to, int weight) {
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 1) return;

    float ux = dx / len;
    float uy = dy / len;

    float sx = from.x + ux * (NODE_RADIUS + 5);
    float sy = from.y + uy * (NODE_RADIUS + 5);

    float ex = to.x - ux * (NODE_RADIUS + 18);
    float ey = to.y - uy * (NODE_RADIUS + 18);

    DrawLineEx((Vector2){sx, sy}, (Vector2){ex, ey}, 3, COL_ROAD);

    float tip_x = to.x - ux * (NODE_RADIUS + 4);
    float tip_y = to.y - uy * (NODE_RADIUS + 4);

    draw_arrowhead(tip_x, tip_y, ux, uy, COL_ARROW);

    char buf[20];
    sprintf(buf, "%d", weight);

    DrawText(buf, (int)((sx+ex)/2), (int)((sy+ey)/2), 14, COL_BADGE_TEXT);
}

static void draw_district(NodeInfo info, int id) {
    DrawCircle(info.x, info.y, NODE_RADIUS, COL_NODE_FILL);

    char buf[10];
    sprintf(buf, "%d", id);

    DrawText(buf, info.x - 5, info.y - 5, 14, WHITE);
}

void draw_graph(const Graph *g, const NodeInfo *info) {
    draw_grid();

    for (int i = 0; i < g->num_nodes; i++) {
        Edge *e = g->nodes[i].head;
        while (e) {
            draw_road(info[i], info[e->dest], e->weight);
            e = e->next;
        }
    }

    for (int i = 0; i < g->num_nodes; i++) {
        draw_district(info[i], i);
    }
}