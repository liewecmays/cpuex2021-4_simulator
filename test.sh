#!/bin/bash

while getopts f: OPT
do
    case $OPT in
        f) FILENAME=$OPTARG;;
    esac
done

make
echo -e ""

cp source/"${FILENAME}.s" assembler/source/"${FILENAME}.s"
cd assembler
./asm -f $FILENAME
cd ../
echo -e ""

cp assembler/out/$FILENAME simulator/code/$FILENAME
cd simulator
./sim -f $FILENAME