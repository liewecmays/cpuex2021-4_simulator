CC := g++
OUTPUT_OPTION := -I./ -std=c++17 -Wall -O3 -mtune=native -march=native -pthread

default: sim server

all: clean sim sim2 prof prof2 server fpu_test

sim: util.o common.o fpu.o sim.o
	$(CC) $(OUTPUT_OPTION) -o $@ $^ -lboost_program_options

sim2: common.hpp fpu.hpp sim2.hpp sim2.cpp
	$(CC) $(OUTPUT_OPTION) -o $@ common.hpp fpu.hpp sim2.cpp -lboost_program_options

prof: util.prof.o common.prof.o fpu.prof.o sim.prof.o
	$(CC) $(OUTPUT_OPTION) -pg -o $@ $^ -lboost_program_options

prof2: common.hpp fpu.hpp sim2.hpp sim2.cpp
	$(CC) $(OUTPUT_OPTION) -pg -o $@ common.hpp fpu.hpp sim2.cpp -lboost_program_options

server: util.o common.o server.o
	$(CC) $(OUTPUT_OPTION) -o $@ $^

fpu_test: util.o common.o fpu.o fpu_test.o
	$(CC) $(OUTPUT_OPTION) -o $@ $^ -lboost_program_options

%.o: %.cpp %.hpp
	$(CC) $(OUTPUT_OPTION) -o $@ -c $<

%.prof.o: %.cpp %.hpp
	$(CC) $(OUTPUT_OPTION) -pg -o $@ -c $<

clean:
	rm -f sim sim2 server fpu_test prof prof2 *.o

clean-info:
	rm -f info/*.md info/*.csv

clean-out:
	rm -f out/*.ppm out/*.bin

clean-all: clean clean-info clean-out
