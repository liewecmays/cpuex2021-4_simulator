CC := g++
OUTPUT_OPTION := -std=c++17 -Wall -O3 -pthread

all: sim server

sim: common.o util.o sim.o
	$(CC) $(OUTPUT_OPTION) -o $@ $^

server: common.o util.o server.o
	$(CC) $(OUTPUT_OPTION) -o $@ $^

.cc.o:

clean:
	rm -f sim server *.o
