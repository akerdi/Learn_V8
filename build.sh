#!/bin/bash

# ./build [build-type]
# <build-type> Could be one of [Debug|Release], Default "Release"

BUILD_DIR=cmake-build
BUILD_TYPE=Release
if [ "$1" ]; then
  if [ "$1" == "Debug" ]; then
    BUILD_TYPE=Debug
  fi
fi

COMPILER_OPT="cmake -S . -B $BUILD_DIR-$BUILD_TYPE"
COMPILER_OPT="$COMPILER_OPT -DCMAKE_BUILD_TYPE=$BUILD_TYPE"

eval $COMPILER_OPT
cmake --build $BUILD_DIR-$BUILD_TYPE --verbose
