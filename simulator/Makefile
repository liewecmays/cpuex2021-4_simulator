CC := g++
OUTPUT_OPTION := -I./ -std=c++20 -Wall -O3 -mtune=native -march=native

default: sim sim-d sim2

all: clean sim sim-d sim2 server fpu_test

sim: params.hpp common.hpp unit.hpp fpu.hpp sim.hpp sim.cpp
	$(CC) $(OUTPUT_OPTION) -o $@ sim.cpp -lboost_program_options

sim-d: params.hpp common.hpp unit.hpp fpu.hpp transmission.hpp sim.hpp sim.cpp
	$(CC) $(OUTPUT_OPTION) -D DETAILED -o $@ sim.cpp -pthread -lboost_program_options

sim2: params.hpp common.hpp unit.hpp fpu.hpp config.hpp sim2.hpp sim2.cpp
	$(CC) $(OUTPUT_OPTION) -o $@ sim2.cpp -lboost_program_options

prof: params.hpp common.hpp unit.hpp fpu.hpp sim.hpp sim.cpp
	$(CC) $(OUTPUT_OPTION) -pg -o $@ sim.cpp -lboost_program_options

prof2: params.hpp common.hpp unit.hpp fpu.hpp config.hpp sim2.hpp sim2.cpp
	$(CC) $(OUTPUT_OPTION) -pg -o $@ sim2.cpp -lboost_program_options

server: params.hpp common.hpp server.hpp server.cpp
	$(CC) $(OUTPUT_OPTION) -o $@ server.cpp -pthread

fpu_test: params.hpp common.hpp fpu.hpp fpu_test.hpp fpu_test.cpp
	$(CC) $(OUTPUT_OPTION) -o $@ fpu_test.cpp -lboost_program_options

clean:
	rm -f sim sim-d sim2 server fpu_test prof prof2 *.o

clean-info:
	rm -f info/*.md info/*.csv

clean-out:
	rm -f out/*.ppm out/*.bin out/*.txt

clean-all: clean clean-info clean-out
