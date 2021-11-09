#!/bin/bash

FILENAME=""
IS_OUT=""
IS_DEBUG=false
PORT=""
IS_BOOTLOADING=""
MEMORY=""
IS_SKIP=""
IS_RAYTRACING=""
while getopts f:odp:bm:sr OPT
do
    case $OPT in
        f) FILENAME=$OPTARG;;
        o) IS_OUT="-o";;
        d) IS_DEBUG=true;;
        p) PORT="-p ${OPTARG}";;
        b) IS_BOOTLOADING="-b";;
        m) MEMORY="-m ${OPTARG}";;
        s) IS_SKIP="-s";;
        r) IS_RAYTRACING="-r"
    esac
done

make || exit 1
echo -e ""
cp source/"${FILENAME}.s" assembler/source/"${FILENAME}.s" || exit 1
cd assembler || exit 1

if "${IS_DEBUG}"; then
    ./asm -d -f $FILENAME $IS_SKIP || exit 1
    cd ../ || exit 1
    cp assembler/out/"${FILENAME}.dbg" simulator/code/"${FILENAME}.dbg" || exit 1
    cd simulator || exit 1
    rlwrap ./sim -d -f $FILENAME $IS_OUT $PORT $IS_BOOTLOADING $MEMORY $IS_SKIP $IS_RAYTRACING || exit 1
else
    ./asm -f $FILENAME $IS_SKIP || exit 1
    cd ../ || exit 1
    cp assembler/out/$FILENAME simulator/code/$FILENAME || exit 1
    cd simulator || exit 1
    ./sim -f $FILENAME $IS_OUT $PORT $IS_BOOTLOADING $MEMORY $IS_SKIP $IS_RAYTRACING || exit 1
fi
