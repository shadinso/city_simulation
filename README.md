# City Simulation

## Team Members

* Shadin Sorkhi (shadinso)
* Alaa Eweisat (alaa-eweisat)
* Dania Hammar (daniaha2005)
* Rama Sublaban (ramasu22)

## Project Description

This project simulates a city road network using graphs and Dijkstra's shortest path algorithm.
Travelers (processes) move concurrently through the graph from source to destination.
The simulation demonstrates process management, IPC, synchronization, and scheduling algorithms in C using Unix APIs.

---

## Repository Structure

graph.c / graph.h - Graph implementation

dijkstra.c / dijkstra.h - Shortest path algorithm

renderer.c / renderer.h - GUI rendering

main.c - Simulation logic for Milestones 2–6

main_m4.c - Separate Milestone 4 implementation

main_m7.c - Milestone 7 implementation (scheduling algorithms)

Makefile - Build targets

README.md - Project documentation

---

## Build & Run Instructions

### Milestone 1 — Dijkstra CLI

```bash
make milestone1
./dijkstra <graph_file>
```

### Milestone 2 — Graph GUI (static display)

```bash
make milestone2
./sim city.txt
```

### Milestone 3 — Animated Traveler

```bash
make milestone3
./sim city.txt
```

### Milestone 4 — Multiple Travelers

```bash
make milestone4
./sim city_m4.txt
```

### Milestone 5 — IPC Using Pipes

```bash
make milestone5
./sim city_m5.txt
```

### Milestone 6 — Node Synchronization (Semaphores)

```bash
make milestone6
./sim city_m6.txt
```

### Milestone 7 — Scheduling Algorithms (FCFS and SJF)

```bash
make milestone7
./sim -schd fcfs city_m7.txt
./sim -schd sjf city_m7.txt
```

### Clean Build Artifacts

```bash
make clean
```

---

## Input File Format

```text
N M
src dst weight
(repeated M times)

K
src dst
(repeated K times)
```

Where:

* N = number of nodes
* M = number of edges
* K = number of travelers

---

## Milestone Descriptions

### Milestone 1

Implemented a directed weighted graph using an adjacency list.
The graph is loaded from a text file and shortest paths are computed using Dijkstra's algorithm with a custom min-heap implementation.

Features:

* Directed weighted graph
* Dijkstra shortest path
* Custom priority queue (min-heap)
* Error handling for invalid inputs and negative weights

### Milestone 2

Added graphical visualization using the Raylib library.

Features:

* Graph drawing
* Directed edges and weights
* Automatic node layout
* Node labels and edge labels

### Milestone 3

Added an animated traveler moving along the shortest path.

Features:

* Traveler animation
* Waiting inside nodes
* Edge traversal according to weight
* Interactive GUI

### Milestone 4

Extended the simulation to multiple travelers using forked processes.

Features:

* Multiple child processes
* Parent-managed GUI
* Unique color per traveler
* Process creation and termination

### Milestone 5 — IPC Mechanism (Pipes)

Each child process computes its own shortest path and communicates with the parent through a dedicated pipe.

Message types:

* MSG_NODE
* MSG_FINISHED

The parent receives updates and displays traveler progress in real time.

### Milestone 6 — Synchronization Mechanism (POSIX Semaphores)

Added synchronization to guarantee that only one traveler can occupy a node at a time.

Features:

* POSIX named semaphores
* Mutual exclusion per node
* Waiting travelers displayed in GUI
* Prevention of node collisions

Additional message types:

* MSG_WAITING
* MSG_TRAVELING

### Milestone 7 — Scheduling Algorithms

The random order of node entry was replaced by parent-managed scheduling.

#### How It Works

When a traveler wants to enter a node:

1. The child sends MSG_WAITING.
2. The child blocks on a private ACK pipe.
3. The parent places the traveler into the node's waiting queue.
4. When the node becomes available, the scheduler selects the next traveler.
5. The parent sends an ACK message to allow entry.

Each node maintains its own waiting queue.

#### FCFS (First Come First Served)

Travelers enter in the same order they requested access.

Selection rule:

* Earliest arrival sequence wins.

#### SJF (Shortest Job First)

Travelers with the shortest remaining route are prioritized.

Selection rule:

* Smallest remaining path length wins.
* Arrival order is used as a tie breaker.

#### Runtime Selection

```bash
./sim -schd fcfs city_m7.txt
./sim -schd sjf city_m7.txt
```

#### GUI Support

The active scheduling algorithm is displayed:

* In the window title
* In the GUI header
* In a scheduler badge at the bottom-right corner

#### FCFS vs SJF Comparison

FCFS serves travelers strictly according to arrival order.

SJF prioritizes travelers with shorter remaining routes, reducing waiting times for short paths while potentially increasing waiting times for longer paths.

Using the same input file, the difference between both algorithms can be clearly observed in the order travelers enter contested nodes.

---

## Dependencies

* Raylib
* GCC
* POSIX APIs
* Linux / Unix environment

---

## Final Submission

The final submission includes:

* Milestones 1–7
* Makefile with all required targets
* README file
* GitHub repository
* Final tag:

```bash
git tag final
git push origin final
```

* Demonstration video showing:

```bash
./sim -schd fcfs city_m7.txt
./sim -schd sjf city_m7.txt
```

and highlighting the behavioral difference between the two scheduling algorithms.
