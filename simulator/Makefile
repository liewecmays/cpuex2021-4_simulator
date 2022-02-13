CC := g++
OUTPUT_OPTION := -I./ -std=c++20 -Wall -O3 -mtune=native -march=native -pthread

default: sim sim-d sim2

all: clean sim sim2 server fpu_test

sim: common.hpp unit.hpp fpu.hpp sim.hpp sim.cpp
	$(CC) $(OUTPUT_OPTION) -o $@ sim.cpp -lboost_program_options

sim-d: common.hpp unit.hpp fpu.hpp sim.hpp sim.cpp
	$(CC) $(OUTPUT_OPTION) -D DETAILED -o $@ sim.cpp -lboost_program_options

sim2: common.hpp unit.hpp fpu.hpp config.hpp sim2.hpp sim2.cpp
	$(CC) $(OUTPUT_OPTION) -o $@ sim2.cpp -lboost_program_options

prof: common.hpp unit.hpp fpu.hpp sim.hpp sim.cpp
	$(CC) $(OUTPUT_OPTION) -pg -o $@ sim.cpp -lboost_program_options

prof2: common.hpp unit.hpp fpu.hpp config.hpp sim2.hpp sim2.cpp
	$(CC) $(OUTPUT_OPTION) -pg -o $@ sim2.cpp -lboost_program_options

server: common.hpp server.hpp server.cpp
	$(CC) $(OUTPUT_OPTION) -o $@ server.cpp

fpu_test: common.hpp fpu.hpp fpu_test.hpp fpu_test.cpp
	$(CC) $(OUTPUT_OPTION) -o $@ fpu_test.cpp -lboost_program_options

clean:
	rm -f sim sim-d sim2 server fpu_test prof prof2 *.o

clean-info:
	rm -f info/*.md info/*.csv

clean-out:
	rm -f out/*.ppm out/*.bin

clean-all: clean clean-info clean-out
