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
    cp source/"${FILENAME}.s" assembler/source/"${FILENAME}.s" || exit 1
    cd assembler
    ./asm -f $FILENAME -d || exit 1
    cd ../
    cp assembler/out/"${FILENAME}.dbg" simulator/code/"${FILENAME}.dbg" || exit 1
    cd simulator || exit 1
    ./sim -f $FILENAME -d || exit 1
else
    make -s
    cp source/"${FILENAME}.s" assembler/source/"${FILENAME}.s" || exit 1
    cd assembler
    ./asm -f $FILENAME || exit 1
    cd ../
    cp assembler/out/$FILENAME simulator/code/$FILENAME || exit 1
    cd simulator || exit 1
    ./sim -f $FILENAME
fi
