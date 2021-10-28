CC = g++
CFLAGS = -std=c++17 -Wall -O2 -pthread

all: sim server

sim: common.o util.o sim.o
	$(CC) $(CFLAGS) -o sim common.o util.o sim.o -lboost_system

server: common.o util.o server.o
	$(CC) $(CFLAGS) -o server common.o util.o server.o -lboost_system

.c.o:
	$(CC) -c $^

clean:
	rm -f sim server *.o
