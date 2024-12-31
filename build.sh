#!/bin/bash

gcc -c ./s_buffer.c -o ./s_buffer.o -fPIC -v && \
gcc -shared ./s_buffer.o -o ./libsbuffer.so -v && \
rm -rf ./s_buffer.o
