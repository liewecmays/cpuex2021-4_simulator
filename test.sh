#!/bin/bash

while getopts f: OPT
do
    case $OPT in
        f) FILENAME=$OPTARG;;
    esac
done

make
echo -e ""

cd assembler
./asm -f $FILENAME
cd ../
echo -e ""

mv assembler/out/$FILENAME simulator/code/$FILENAME
cd simulator
./sim -f $FILENAME