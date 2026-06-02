all:
	mkdir -p build
	cd build && cmake .. && make

run:
	./build/sim city.txt

clean:
	rm -rf build

