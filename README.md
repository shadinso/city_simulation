# City Simulation

## Team Members

- Shadin Sorkhi (shadinso)
- Alaa Eweisat (alaa-eweisat)
- Dania Hammar (daniaha2005)
- Rama Sublaban (ramasu22)

## Project Description

This project simulates a city road network using graphs and Dijkstra's shortest path algorithm.
Travelers (processes) move concurrently through the graph from source to destination.
The simulation demonstrates process management, IPC, and synchronization in C using Unix APIs.

---

## Repository Structure

graph.c / graph.h - Graph implementation

dijkstra.c / dijkstra.h - Shortest path algorithm

renderer.c / renderer.h - GUI rendering

main.c - Simulation logic

Makefile - Build targets

README.md - Project documentation

---

## Build & Run Instructions

### Milestone 1 — Dijkstra CLI

make milestone1
./dijkstra <graph_file>

### Milestone 2 — Graph GUI (static display)

make milestone2
./sim city.txt

### Milestone 3 — Animated traveler

make milestone3
./sim city.txt

### Milestone 4 — Multiple travelers (fork, parent manages GUI)

make milestone4
./sim city_m4.txt

### Milestone 5 — IPC (children compute own paths and report via pipes)

make milestone5
./sim city_m5.txt

### Milestone 6 — Node synchronization (semaphores)

make milestone6
./sim city_m5.txt

### Clean build artifacts

make clean

---

## Input File Format

N M
src dst weight   (repeated M times)
K
src dst          (K traveler definitions)

---

## Milestone Descriptions

### Milestone 1

Implemented a directed weighted graph using an adjacency list. Reads the graph from a text file. Implements Dijkstra's algorithm with a custom min-heap. Handles no-path, same src/dst, and negative weight errors.

### Milestone 2

Added graphical display using the Raylib library. The graph is drawn with labeled nodes, directed edges (arrows), and edge weights. Node positions are fixed in a city-themed layout.

### Milestone 3

Added an animated traveler that moves along the shortest path. The traveler waits 1 second at each intermediate node and travels each edge in (weight × 300ms). A play/stop button controls the animation.

### Milestone 4

Extended to multiple travelers running concurrently. The parent process computes each traveler's Dijkstra path, forks child processes, and manages the GUI. Each traveler is shown in a unique color. Children sleep while the parent animates them. The parent sends SIGTERM to each child upon path completion.

### Milestone 5 — IPC Mechanism: Pipes

Each child process independently computes its own Dijkstra path and travels along it. At each node arrival, the child sends a structured message to the parent via a Unix pipe (one pipe per traveler). The parent reads messages in non-blocking mode inside the GUI loop and prints a log entry per event:

[PID=XXXX] arrived at node N | next node: M
[PID=XXXX] arrived at node N | DESTINATION
[PID=XXXX] finished

Pipes were chosen because they are simple, unidirectional, and well-suited for the one-to-one child-to-parent communication pattern used in this project.

### Milestone 6 — Synchronization Mechanism: POSIX Named Semaphores

Added a constraint: at most one traveler inside a node at any time.

Each node i has a POSIX named semaphore /city_node_i initialized to 1. Before entering a node, a child process calls sem_wait() on that node's semaphore. If the node is currently occupied, the child blocks and sends a MSG_WAITING message to the parent. The parent updates the GUI and displays the traveler in yellow with a W label.

Once the semaphore becomes available, the traveler enters the node and sends a MSG_ENTERED message. After spending the required time inside the node, the traveler releases the semaphore using sem_post(), allowing another waiting traveler to enter.

This guarantees:

- Mutual exclusion: no two travelers can occupy the same node simultaneously.
- Waiting travelers are eventually allowed to enter once the node becomes available.
- The GUI clearly displays waiting travelers using a yellow circle and a W# label.

#### IPC Message Types

- MSG_NODE – traveler arrived at a node.
- MSG_FINISHED – traveler completed its route.
- MSG_WAITING – traveler waiting outside a node.
- MSG_ENTERED – traveler entered a node after acquiring the semaphore.

---

## Dependencies

- Raylib (must be installed on the system)
- Standard C99 + POSIX
