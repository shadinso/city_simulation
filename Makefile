.PHONY: all clean run milestone1 milestone2 milestone3 milestone4 milestone5 milestone6

all: milestone6

milestone1:
	mkdir -p build
	cd build && cmake .. && make
	cp build/dijkstra ./dijkstra

milestone2:
	mkdir -p build
	cd build && cmake .. && make
	cp build/sim ./sim

milestone3:
	mkdir -p build
	cd build && cmake .. && make
	cp build/sim ./sim
	cp build/dijkstra ./dijkstra

milestone4:
	mkdir -p build
	cd build && cmake .. && make
	cp build/sim ./sim
	cp build/dijkstra ./dijkstra

milestone5:
	mkdir -p build
	cd build && cmake .. && make
	cp build/sim ./sim
	cp build/dijkstra ./dijkstra

milestone6:
	mkdir -p build
	cd build && cmake .. && make
	cp build/sim ./sim
	cp build/dijkstra ./dijkstra

run:
	./sim city_m5.txt

clean:
	rm -rf build sim dijkstra
