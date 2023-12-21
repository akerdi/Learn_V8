#!/bin/bash

# ./build [build-type]
# <build-type> Could be one of [Debug|Release]

BUILD_DIR=cmake-build

if [ "$1" ]; then
  if [ "$1" == "Debug" ]; then
    BUILD_DIR="$BUILD_DIR-debug"
  else
    BUILD_DIR="$BUILD_DIR-release"
  fi
else
  BUILD_DIR="$BUILD_DIR-debug"
fi

COMPILER_OPT="cmake -S . -B $BUILD_DIR"

if [ "$1" ]; then
  COMPOLER_OPT="$COMPILER_OPT -DCMAKE_BUILD_TYPE=$1"
fi

eval $COMPILER_OPT
cmake --build $BUILD_DIR
