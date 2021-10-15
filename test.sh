#!/bin/bash

FILENAME=""
IS_DEBUG=false
while getopts f:d OPT
do
    case $OPT in
        f) FILENAME=$OPTARG;;
        d) IS_DEBUG=true;;
    esac
done

if "${IS_DEBUG}"; then
    make
    echo -e ""
    cp source/"${FILENAME}.s" assembler/source/"${FILENAME}.s"
    cd assembler
    ./asm -f $FILENAME -d
    cd ../
    cp assembler/out/"${FILENAME}.dbg" simulator/code/"${FILENAME}.dbg"
    cd simulator
    ./sim -f $FILENAME -d
else
    make -s
    cp source/"${FILENAME}.s" assembler/source/"${FILENAME}.s"
    cd assembler
    ./asm -f $FILENAME
    cd ../
    cp assembler/out/$FILENAME simulator/code/$FILENAME
    cd simulator
    ./sim -f $FILENAME
fi
