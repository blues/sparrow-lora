#!/bin/sh
[ -d cmake-build-debug ] || mkdir cmake-build-debug
[ -d note-c ] || git clone https://github.com/blues/note-c
cd cmake-build-debug
cmake -DCMAKE_TOOLCHAIN_FILE=../arm-gcc-toolchain.cmake -DCMAKE_BUILD_TYPE=Debug .. && cmake --build . -- -j 4