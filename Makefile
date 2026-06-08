all: milestone3

milestone1:
	mkdir -p build
	cd build && cmake .. && make
	cp build/sim ./dijkstra

milestone2:
	mkdir -p build
	cd build && cmake .. && make
	cp build/sim ./sim

milestone3:
	mkdir -p build
	cd build && cmake .. && make
	cp build/sim ./sim
	cp build/sim ./dijkstra

run:
	./sim city.txt

clean:
	rm -rf build sim dijkstra
