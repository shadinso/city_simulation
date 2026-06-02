all:
	mkdir -p build
	cd build && cmake .. && make

milestone1:
	mkdir -p build
	cd build && cmake .. && make

milestone2:
	mkdir -p build
	cd build && cmake .. && make

milestone3:
	mkdir -p build
	cd build && cmake .. && make

run:
	./build/sim city.txt

clean:
	rm -rf build
