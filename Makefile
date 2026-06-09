milestone5:
	mkdir -p build
	cd build && cmake .. && make
	cp build/sim ./sim
	cp build/dijkstra ./dijkstra

