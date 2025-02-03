#!/bin/bash

cd $(dirname $0)

DEMO_ROOT=$PWD
VENDOR_ROOT=$DEMO_ROOT/vendor
SDL_ROOT=$VENDOR_ROOT/SDL2

# ==============================================================================
# build SDL
# ==============================================================================
if [[ !(-d $SDL_ROOT/dist) || $1 == "-c" || $1 == "--clean" ]]; then
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
./build.sh -d

# ==============================================================================
# build demo
# ==============================================================================
cd $DEMO_ROOT
gcc ./demo.c -o ./sbuffer-demo -I../ -I./vendor/SDL2/dist/include \
                               -L../ -lsbuffer -L./vendor/SDL2/dist/lib -lSDL2 \
                               -g # FIXME: remove this before production!
