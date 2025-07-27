#!/bin/bash

set -e

# Check for & make directories
test -d _build || mkdir _build
test -d _install || mkdir _install

# Move to build folder
cd _build

# Run CMake Configurations
cmake .. -DCMAKE_INSTALL_PREFIX=../_install -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DBUILD_TOOLS:BOOL=1 -DBUILD_MANGOSD:BOOL=1 -DBUILD_REALMD:BOOL=1 -DSOAP:BOOL=1 -DSCRIPT_LIB_ELUNA:BOOL=1 -DSCRIPT_LIB_SD3:BOOL=1 -DPLAYERBOTS:BOOL=1 -DUSE_STORMLIB:BOOL=1

# Compile the Project
make -j$(nproc)
