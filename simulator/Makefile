all: sim server

sim: util.hpp util.cpp op.hpp op.cpp sim.hpp sim.cpp
	g++ -std=c++17 -Wall -O2 -pthread -o sim util.cpp op.cpp sim.cpp -lboost_system

server: server.cpp
	g++ -std=c++17 -Wall -O2 -pthread -o server server.cpp -lboost_system

clean:
	rm -f sim server