#!/bin/bash

FILENAME=""
IS_INFO_OUT=""
IS_DEBUG=""
PORT=""
IS_BOOTLOADING=""
MEMORY=""
IS_SKIP=""
IS_RAYTRACING=""
IS_BIN=""
while getopts f:idp:bm:sr-: OPT
do
    case $OPT in
        -)
            case $OPTARG in
                boot) IS_BOOTLOADING="--boot";;
            esac;;
        f) FILENAME=$OPTARG;;
        i) IS_INFO_OUT="-i";;
        d) IS_DEBUG="-d";;
        p) PORT="-p ${OPTARG}";;
        b) IS_BIN="-b";;
        m) MEMORY="-m ${OPTARG}";;
        s) IS_SKIP="-s";;
        r) IS_RAYTRACING="-r";;
    esac
done

EXT=""
if test IS_BIN = "-b"; then
    EXT=".bin"
else
    if test IS_DEBUG = "-d"; then
        EXT=".dbg"
    fi
fi

cp source/"${FILENAME}.s" assembler/source/"${FILENAME}.s" || exit 1
cd assembler || exit 1
./asm -f $FILENAME $IS_DEBUG $IS_BOOTLOADING $IS_SKIP $IS_BIN || exit 1
cd ../ || exit 1
cp assembler/out/$FILENAME$EXT simulator/code/$FILENAME$EXT || exit 1
cd simulator || exit 1
rlwrap ./sim -f $FILENAME $IS_DEBUG $IS_INFO_OUT $PORT $IS_BOOTLOADING $MEMORY $IS_SKIP $IS_RAYTRACING || exit 1
