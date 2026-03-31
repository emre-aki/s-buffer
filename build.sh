#!/bin/bash

# Configure Git to use custom hooks from the repository
git config core.hooksPath .githooks

SB_DEBUG=""
SB_VERBOSE=""

while [[ $# -gt 0 ]]; do
    key="$1"

    case $key in
    -d|--debug)
        SB_DEBUG="-DSB_DEBUG"
        shift
        ;;
    -h|--help)
        echo "Options:
-d,    --debug    Build in debug mode
-h,    --help     Display this help message and exit
-v,    --verbose  Enable verbose logging"
        exit 0
        ;;
    -v|--verbose)
        SB_VERBOSE="-DSB_VERBOSE"
        shift
        ;;
    -*)
        echo "fatal: Unknown argument $1"
        exit 1
        ;;
    esac
done

cd "$(dirname "$0")"

ROOT="$(pwd -P)"
DIST_ROOT="$ROOT/dist"

rm -rfv "$DIST_ROOT"
mkdir "$DIST_ROOT"

if [[ -z $SB_DEBUG ]]; then
    gcc -c $SB_VERBOSE ./s_buffer.c -o ./s_buffer.o -fPIC -v &&      \
    gcc -shared ./s_buffer.o -o "$DIST_ROOT/libsbuffer.so" -lm -v && \
    rm -rf ./s_buffer.o
else
    gcc -shared $SB_DEBUG $SB_VERBOSE \
        ./s_buffer.c                  \
        -o "$DIST_ROOT/libsbuffer.so" \
        -lm -fPIC -v -g
fi
