CC = g++
CFLAGS = -std=c++17 -Wall -O2 -pthread

all: sim server

sim: util.o op.o sim.o
	$(CC) $(CFLAGS) -o sim util.o op.o sim.o -lboost_system

server: util.o server.o
	$(CC) $(CFLAGS) -o server util.o server.o -lboost_system

.c.o:
	$(CC) -c $^

clean:
	rm -f sim server *.o
