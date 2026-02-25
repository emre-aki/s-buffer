#!/bin/bash

CLEAN=""
SB_DEBUG=""
SB_VERBOSE=""

while [[ $# -gt 0 ]]; do
    key="$1"

    case $key in
    -c|--clean)
        CLEAN="true"
        shift
        ;;
    -d|--debug)
        SB_DEBUG="-d"
        shift
        ;;
    -h|--help)
        echo "Options:
-c,    --clean    Rebuild all dependencies
-d,    --debug    Build in debug mode
-h,    --help     Display this help message and exit
-v,    --verbose  Enable verbose logging"
        exit 0
        ;;
    -v|--verbose)
        SB_VERBOSE="-v"
        shift
        ;;
    -*)
        echo "fatal: Unknown argument $1"
        exit 1
        ;;
    esac
done

cd "$(dirname "$0")"

DEMO_ROOT="$(pwd -P)"
VENDOR_ROOT="$DEMO_ROOT/vendor"
SDL_ROOT="$VENDOR_ROOT/SDL2"

# ==============================================================================
# build SDL
# ==============================================================================
if [[ !(-d $SDL_ROOT/dist && -z $CLEAN) ]]; then
    git submodule update --init --recursive # update the SDL submodule

    # clear the build directory
    cd "$SDL_ROOT"
    rm -rf ./build
    mkdir ./build

    # make a dist directory to serve dynamic and static libraries
    rm -rf ./dist
    mkdir ./dist

    # configure, build, and install SDL
    cd ./build
    ../configure --prefix="$SDL_ROOT/dist" --enable-static=yes
    make
    make install
fi

# ==============================================================================
# build s-buffer
# ==============================================================================
cd "$DEMO_ROOT/.."

./build.sh $SB_DEBUG $SB_VERBOSE

# ==============================================================================
# build demo
# ==============================================================================
cd "$DEMO_ROOT"

if [[ -z $SB_DEBUG ]]; then
    gcc ./*.c -o ./sbuffer-demo -I../ -L../dist -lsbuffer    \
                                -I./vendor/SDL2/dist/include \
                                -L./vendor/SDL2/dist/lib     \
                                -lSDL2
else
    gcc ./*.c -o ./sbuffer-demo -I../ -L../dist -lsbuffer    \
                                -I./vendor/SDL2/dist/include \
                                -L./vendor/SDL2/dist/lib     \
                                -lSDL2                       \
                                -DSB_DEBUG -g
fi
