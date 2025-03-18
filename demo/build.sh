#!/bin/bash

CLEAN=""
DDEBUG=""

while [[ $# -gt 0 ]]; do
    key="$1"

    case $key in
    -d|--debug)
        DG="-g"
        DV="-v"
        DDEBUG="-DDEBUG"
        shift
        ;;
    -c|--clean)
        CLEAN="true"
        shift
        ;;
    -h|--help)
        echo "Options:
-d,    --debug  Build in debug mode
-c,    --clean  Rebuild all dependencies
-h,    --help   Display this help message and exit"
        exit 0
        ;;
    -*)
        echo "fatal: Unknown argument $1"
        exit 1
        ;;
    esac
done

cd $(dirname $0)

DEMO_ROOT=$PWD
VENDOR_ROOT=$DEMO_ROOT/vendor
SDL_ROOT=$VENDOR_ROOT/SDL2

# ==============================================================================
# build SDL
# ==============================================================================
if [[ !(-d $SDL_ROOT/dist && -z $CLEAN) ]]; then
    git submodule update --init --recursive # update the SDL submodule

    # clear the build directory
    cd $SDL_ROOT
    rm -rf ./build
    mkdir ./build

    # make a dist directory to serve dynamic and static libraries
    rm -rf ./dist
    mkdir ./dist

    # configure, build, and install SDL
    cd ./build
    ../configure --prefix=$SDL_ROOT/dist --enable-static=yes
    make
    make install
fi

# ==============================================================================
# build s-buffer
# ==============================================================================
cd $DEMO_ROOT/..

if [[ -z $DDEBUG ]]; then
    ./build.sh
else
    ./build.sh -d
fi

# ==============================================================================
# build demo
# ==============================================================================
cd $DEMO_ROOT

if [[ -z $DDEBUG ]]; then
    gcc ./demo.c -o ./sbuffer-demo -I../ -I./vendor/SDL2/dist/include \
                                   -L../ -lsbuffer                    \
                                   -L./vendor/SDL2/dist/lib -lSDL2
else
    gcc ./demo.c -o ./sbuffer-demo -I../ -I./vendor/SDL2/dist/include \
                                   -L../ -lsbuffer                    \
                                   -L./vendor/SDL2/dist/lib -lSDL2    \
                                   $DDEBUG -g
fi
