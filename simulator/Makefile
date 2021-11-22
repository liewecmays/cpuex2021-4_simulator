CC := g++
OUTPUT_OPTION := -I./ -std=c++17 -Wall -O3 -mtune=native -march=native -pthread

default: sim server

all: clean sim server fpu_test

sim: util.o common.o sim.o
	$(CC) $(OUTPUT_OPTION) -o $@ $^ -lboost_program_options

server: util.o common.o server.o
	$(CC) $(OUTPUT_OPTION) -o $@ $^

fpu_test: util.o common.o fpu.o fpu_test.o
	$(CC) $(OUTPUT_OPTION) -o $@ $^

%.o: %.cpp %.hpp
	$(CC) $(OUTPUT_OPTION) -o $@ -c $<

clean:
	rm -f sim server fpu_test *.o
