#!/bin/bash

cd $(dirname $0)

ROOT=$PWD
DIST_ROOT=$ROOT/dist

rm -rfv $DIST_ROOT
mkdir $DIST_ROOT


if [[ $1 == "-d" ]]; then
    gcc -shared -DDEBUG ./s_buffer.c -o $DIST_ROOT/libsbuffer.so -fPIC -v -g
else
    gcc -c ./s_buffer.c -o ./s_buffer.o -fPIC -v &&            \
    gcc -shared ./s_buffer.o -o $DIST_ROOT/libsbuffer.so -v && \
    rm -rf ./s_buffer.o
fi
