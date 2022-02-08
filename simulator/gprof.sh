#!/bin/bash

FILENAME=""
VERSION=""
EXE=""
OUT=""

while getopts f:v: OPT
do
    case $OPT in
        f) FILENAME=$OPTARG;;
        v) VERSION=$OPTARG;;
    esac
done

if [ "$VERSION" = "1" ]; then
    EXE="./prof"
    OUT="gmon.txt"
else
    if [ "$VERSION" = "2" ]; then
        EXE="./prof2"
        OUT="gmon2.txt"
    fi
fi

$EXE -f $FILENAME -r --preload
gprof $EXE gmon.out > $OUT
