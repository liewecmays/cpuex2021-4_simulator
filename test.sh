#!/bin/bash

IS_SECOND=""
FILENAME=""
IS_INFO_OUT=""
IS_DEBUG=""
IS_STAT=""
PORT=""
# IS_BOOTLOADING=""
MEMORY=""
IS_SKIP=""
IS_RAYTRACING=""
IS_BIN=""
IS_PRELOADING=""
IS_IEEE=""
IS_GSHARE=""
IS_CACHE=""
IS_CAUTIOUS=""
while getopts 2f:bdim:srp:gc-: OPT
do
    case $OPT in
        -)
            case $OPTARG in
                ieee) IS_IEEE="--ieee";;
                preload) IS_PRELOADING="--preload";;
                # boot) IS_BOOTLOADING="--boot";;
                stat) IS_STAT="--stat";;
                cautious) IS_CAUTIOUS="--cautious"
            esac;;
        2) IS_SECOND="2nd";;
        f) FILENAME=$OPTARG;;
        d) IS_DEBUG="-d";;
        b) IS_BIN="-b";;
        i) IS_INFO_OUT="-i";;
        m) MEMORY="-m ${OPTARG}";;
        s) IS_SKIP="-s";;
        r) IS_RAYTRACING="-r";;
        p) PORT="-p ${OPTARG}";;
        g) IS_GSHARE="-g";;
        c) IS_CACHE="-c 0";;
    esac
done

EXT=""
if [ "${IS_BIN}" != "" ]; then
    EXT=".bin"
else
    if [ "${IS_DEBUG}" != "" ]; then
        EXT=".dbg"
    fi
fi

cp source/"${FILENAME}.s" assembler/source/"${FILENAME}.s" || exit 1
cd assembler || exit 1
./asm -f $FILENAME $IS_DEBUG $IS_SKIP $IS_BIN || exit 1
cd ../ || exit 1
cp assembler/out/$FILENAME$EXT simulator/code/$FILENAME$EXT || exit 1
cd simulator || exit 1


if [ "${IS_SECOND}" != "" ]; then
    rlwrap ./sim2 -f $FILENAME $IS_BIN $IS_DEBUG $IS_INFO_OUT $MEMORY $IS_IEEE $IS_PRELOADING $IS_RAYTRACING || exit 1
else
    if [ "$PORT" != "" -o "$IS_GSHARE" != "" -o "$IS_CACHE" != "" -o "$IS_STAT" != "" -o "$IS_CAUTIOUS" != "" ]; then
        rlwrap ./sim+ -f $FILENAME $IS_BIN $IS_DEBUG $IS_INFO_OUT $MEMORY $IS_IEEE $IS_SKIP $IS_PRELOADING $IS_RAYTRACING $PORT $IS_BOOTLOADING $IS_GSHARE $IS_CACHE $IS_STAT $IS_CAUTIOUS || exit 1
    else
        rlwrap ./sim -f $FILENAME $IS_BIN $IS_DEBUG $IS_INFO_OUT $IS_SKIP $MEMORY $IS_IEEE $IS_PRELOADING $IS_RAYTRACING || exit 1
    fi
fi
