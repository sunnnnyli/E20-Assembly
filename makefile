all: asm.cpp simcache.cpp
	g++ asm.cpp -o asm.exe
	g++ simcache.cpp -o simcache.exe
run:
	./asm myprog.s > myprog.bin
	./simcache --cache 4,1,1,64,4,4 myprog.bin

clean:
	rm *.exe
	rm *.bin