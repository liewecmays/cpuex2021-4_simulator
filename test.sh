#!/bin/bash

FILENAME=""
IS_DEBUG=false
IS_OUT=false
while getopts f:do OPT
do
    case $OPT in
        f) FILENAME=$OPTARG;;
        d) IS_DEBUG=true;;
        o) IS_OUT=true;;
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
    if "${IS_OUT}"; then
        rlwrap ./sim -f $FILENAME -o -d || exit 1
    else
        rlwrap ./sim -f $FILENAME -d || exit 1
    fi
else
    make -s
    cp source/"${FILENAME}.s" assembler/source/"${FILENAME}.s" || exit 1
    cd assembler
    ./asm -f $FILENAME || exit 1
    cd ../
    cp assembler/out/$FILENAME simulator/code/$FILENAME || exit 1
    cd simulator || exit 1
    if "${IS_OUT}"; then
        rlwrap ./sim -f $FILENAME -o || exit 1
    else
        rlwrap ./sim -f $FILENAME || exit 1
    fi
fi
