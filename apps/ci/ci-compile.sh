#!/bin/bash

set -e

test -d build || mkdir build
cd build

cmake .. \
  -DBUILD_MANGOSD:BOOL=1 \
  -DBUILD_REALMD:BOOL=1 \
  -DBUILD_TOOLS:BOOL=1 \
  -DPLAYERBOTS:BOOL=1 \
  -DSCRIPT_LIB_ELUNA:BOOL=1 \
  -DSCRIPT_LIB_SD3:BOOL=1 \
  -DSOAP:BOOL=1 \
  -DUSE_STORMLIB:BOOL=1

cmake --build . --config Release --parallel 4
