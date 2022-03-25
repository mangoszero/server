#!/bin/bash

set -e

# Check for & make directories
time test -d _build || mkdir _build
time test -d _install || mkdir _install

# Move to build folder
time cd _build

# Run CMake Configurations
time cmake .. -DCMAKE_INSTALL_PREFIX=../_install -DBUILD_TOOLS:BOOL=1 -DBUILD_MANGOSD:BOOL=1 -DBUILD_REALMD:BOOL=1 -DSOAP:BOOL=1 -DSCRIPT_LIB_ELUNA:BOOL=1 -DSCRIPT_LIB_SD3:BOOL=1 -DPLAYERBOTS:BOOL=1 -DUSE_STORMLIB:BOOL=1

# Compile the Project
time make -j 6
