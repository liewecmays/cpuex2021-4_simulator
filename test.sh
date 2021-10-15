#!/bin/bash

while getopts f:d OPT
do
    case $OPT in
        f) FILENAME=$OPTARG;;
        d) IS_DEBUG=true;;
    esac
done

make
echo -e ""

if "${IS_DEBUG}"; then
    cp source/"${FILENAME}.s" assembler/source/"${FILENAME}.s"
    cd assembler
    ./asm -f $FILENAME -d
    cd ../
    echo -e ""

    cp assembler/out/"${FILENAME}.dbg" simulator/code/"${FILENAME}.dbg"
    cd simulator
    ./sim -f $FILENAME -d
else
    cp source/"${FILENAME}.s" assembler/source/"${FILENAME}.s"
    cd assembler
    ./asm -f $FILENAME
    cd ../
    echo -e ""

    cp assembler/out/$FILENAME simulator/code/$FILENAME
    cd simulator
    ./sim -f $FILENAME
fi
