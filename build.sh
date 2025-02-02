#!/bin/bash

if [[ $1 == "-d" ]]; then
    gcc -shared -DDEBUG ./s_buffer.c -o ./libsbuffer.so -fPIC -v -g
else
    gcc -c ./s_buffer.c -o ./s_buffer.o -fPIC -v && \
    gcc -shared ./s_buffer.o -o ./libsbuffer.so -v && \
    rm -rf ./s_buffer.o
fi
