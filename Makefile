all: milestone3

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
run:
	./sim city.txt

clean:
	rm -rf build sim dijkstra

milestone5:
	mkdir -p build
	cd build && cmake .. && make
	cp build/sim ./sim
	cp build/dijkstra ./dijkstra
