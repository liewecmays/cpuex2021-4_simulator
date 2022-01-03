#!/bin/bash

IS_DEBUG=""
PORT=""
while getopts dp: OPT
do
    case $OPT in
        d) IS_DEBUG="-d";;
        p) PORT="-p ${OPTARG}";;
    esac
done

cd simulator || exit 1
rlwrap ./server $IS_DEBUG $PORT || exit 1
