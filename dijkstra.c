//
// Created by alaa eweisat  on 28/04/2026.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"

typedef struct {
    int cost;
    int node;
} HeapItem;

typedef struct {
    HeapItem *data;
    int size;
    int capacity;
} MinHeap;

static MinHeap *heap_create(int capacity) {
    MinHeap *h = (MinHeap *)malloc(sizeof(MinHeap));
    h->data = (HeapItem *)malloc(capacity * sizeof(HeapItem));
    h->size = 0;
    h->capacity = capacity;
    return h;
}

static void heap_free(MinHeap *h) {
    free(h->data);
    free(h);
}

static void heap_swap(MinHeap *h, int i, int j) {
    HeapItem tmp = h->data[i];
    h->data[i] = h->data[j];
    h->data[j] = tmp;
}

static void heap_push(MinHeap *h, int cost, int node) {
    if (h->size == h->capacity) {
        h->capacity *= 2;
        h->data = (HeapItem *)realloc(h->data, h->capacity * sizeof(HeapItem));
    }

    h->data[h->size].cost = cost;
    h->data[h->size].node = node;

    int i = h->size++;

    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->data[parent].cost > h->data[i].cost) {
            heap_swap(h, parent, i);
            i = parent;
        } else {
            break;
        }
    }
}

static HeapItem heap_pop(MinHeap *h) {
    HeapItem top = h->data[0];
    h->data[0] = h->data[--h->size];

    int i = 0;

    while (1) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;

        if (left < h->size && h->data[left].cost < h->data[smallest].cost)
            smallest = left;

        if (right < h->size && h->data[right].cost < h->data[smallest].cost)
            smallest = right;

        if (smallest == i)
            break;

        heap_swap(h, i, smallest);
        i = smallest;
    }

    return top;
}

int dijkstra(const Graph *g, int src, int dst, int **path, int *path_len) {
    int N = g->num_nodes;

    int *dist = (int *)malloc(N * sizeof(int));
    int *prev = (int *)malloc(N * sizeof(int));
    int *visited = (int *)calloc(N, sizeof(int));

    if (!dist || !prev || !visited) {
        free(dist);
        free(prev);
        free(visited);
        return -1;
    }

    for (int i = 0; i < N; i++) {
        dist[i] = INF;
        prev[i] = -1;
    }

    dist[src] = 0;

    if (src == dst) {
        *path = (int *)malloc(sizeof(int));
        if (!*path) {
            free(dist);
            free(prev);
            free(visited);
            return -1;
        }

        (*path)[0] = src;
        *path_len = 1;

        free(dist);
        free(prev);
        free(visited);

        return 0;
    }

    MinHeap *heap = heap_create(N * 2);
    heap_push(heap, 0, src);

    while (heap->size > 0) {
        HeapItem cur = heap_pop(heap);
        int u = cur.node;

        if (visited[u])
            continue;

        visited[u] = 1;

        if (u == dst)
            break;

        Edge *e = g->nodes[u].head;

        while (e) {
            int v = e->dest;
            int new_d = dist[u] + e->weight;

            if (!visited[v] && new_d < dist[v]) {
                dist[v] = new_d;
                prev[v] = u;
                heap_push(heap, new_d, v);
            }

            e = e->next;
        }
    }

    heap_free(heap);

    if (dist[dst] == INF) {
        *path_len = 0;
        *path = NULL;

        free(dist);
        free(prev);
        free(visited);

        return -1;
    }

    int count = 0;
    int cur = dst;

    while (cur != -1) {
        count++;
        cur = prev[cur];
    }

    int *p = (int *)malloc(count * sizeof(int));

    if (!p) {
        free(dist);
        free(prev);
        free(visited);
        return -1;
    }

    cur = dst;

    for (int i = count - 1; i >= 0; i--) {
        p[i] = cur;
        cur = prev[cur];
    }

    int total_cost = dist[dst];

    *path = p;
    *path_len = count;

    free(dist);
    free(prev);
    free(visited);

    return total_cost;
}
