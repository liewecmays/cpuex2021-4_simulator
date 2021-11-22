#!/bin/bash

FILENAME=""
IS_OUT=""
IS_DEBUG=""
DEBUG_EXT=""
PORT=""
IS_BOOTLOADING=""
MEMORY=""
IS_SKIP=""
IS_RAYTRACING=""
IS_BIN=""
BIN_EXT=""
while getopts f:odp:bm:sr-: OPT
do
    case $OPT in
        -)
            case $OPTARG in
                bin) IS_BIN="--bin"; BIN_EXT=".bin";;
            esac;;
        f) FILENAME=$OPTARG;;
        o) IS_OUT="-o";;
        d) IS_DEBUG="-d"; DEBUG_EXT=".dbg";;
        p) PORT="-p ${OPTARG}";;
        b) IS_BOOTLOADING="-b";;
        m) MEMORY="-m ${OPTARG}";;
        s) IS_SKIP="-s";;
        r) IS_RAYTRACING="-r";;
    esac
done

cp source/"${FILENAME}.s" assembler/source/"${FILENAME}.s" || exit 1
cd assembler || exit 1
./asm -f $FILENAME $IS_DEBUG $IS_BOOTLOADING $IS_SKIP $IS_BIN || exit 1
cd ../ || exit 1
cp assembler/out/$FILENAME$DEBUG_EXT$BIN_EXT simulator/code/$FILENAME$DEBUG_EXT$BIN_EXT || exit 1
cd simulator || exit 1
rlwrap ./sim -f $FILENAME $IS_DEBUG $IS_OUT $PORT $IS_BOOTLOADING $MEMORY $IS_SKIP $IS_RAYTRACING || exit 1
