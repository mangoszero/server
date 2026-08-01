#!/bin/bash

set -e

mkdir -p _build _install
cd _build

# PCH is OFF on purpose. A precompiled header supplies declarations the source never
# named, so a green PCH build says nothing about whether a file includes what it uses --
# and that is exactly the class of defect this CI exists to catch.
time cmake .. -DCMAKE_INSTALL_PREFIX=../_install -DBUILD_TOOLS:BOOL=1 -DBUILD_MANGOSD:BOOL=1 -DBUILD_REALMD:BOOL=1 -DSOAP:BOOL=1 -DSCRIPT_LIB_ELUNA:BOOL=1 -DSCRIPT_LIB_SD3:BOOL=1 -DPLAYERBOTS:BOOL=1 -DPCH:BOOL=0

time make -j $(nproc)
time make install
