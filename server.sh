#!/bin/bash

# IS_DEBUG=""
PORT=""
while getopts p: OPT
do
    case $OPT in
        p) PORT="-p ${OPTARG}";;
    esac
done

cd simulator || exit 1
rlwrap ./server $PORT || exit 1
