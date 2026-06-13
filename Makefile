.PHONY: all clean run milestone1 milestone2 milestone3 milestone4 milestone5 milestone6

all: milestone6

build:
	mkdir -p build
	cd build && cmake .. && make

milestone1: build
	cp build/dijkstra ./dijkstra

milestone2: build
	cp build/sim ./sim

milestone3: build
	cp build/sim ./sim
	cp build/dijkstra ./dijkstra

milestone4: build
	cp build/sim ./sim
	cp build/dijkstra ./dijkstra
	@echo "Running Milestone 4 version"

milestone5: build
	cp build/sim ./sim
	cp build/dijkstra ./dijkstra
	@echo "Running Milestone 5 version"

milestone6: build
	cp build/sim ./sim
	cp build/dijkstra ./dijkstra
	@echo "Running Milestone 6 version"

run:
	./sim city_m6.txt

clean:
	rm -rf build sim dijkstra
