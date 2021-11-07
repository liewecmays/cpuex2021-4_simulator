CC := g++
OUTPUT_OPTION := -std=c++17 -Wall -O3 -mtune=native -march=native -pthread

all: sim server

sim: util.o common.o sim.o
	$(CC) $(OUTPUT_OPTION) -o $@ $^

server: util.o common.o server.o
	$(CC) $(OUTPUT_OPTION) -o $@ $^

.cc.o:

clean:
	rm -f sim server *.o
