#!/bin/sh

export CXX=clang++
export CC=clang

DEPENDENCIES_ROOT="/private/tmp/wildmidi-deps/wildmidi-deps"

mkdir build
cd build

cmake \
-D CMAKE_EXE_LINKER_FLAGS="-lz" \
-D CMAKE_PREFIX_PATH="$DEPENDENCIES_ROOT" \
-D CMAKE_OSX_DEPLOYMENT_TARGET="10.9" \
-D CMAKE_OSX_SYSROOT="macosx10.12" \
-D CMAKE_BUILD_TYPE=Release \
-D WANT_OSX_DEPLOYMENT=TRUE \
-G"Unix Makefiles" \
..
