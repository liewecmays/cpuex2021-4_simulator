#!/bin/bash
./prof -f minrt -r --preload
gprof ./prof gmon.out > gmon.txt
