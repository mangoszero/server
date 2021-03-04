#!/bin/bash

set -e

# Check for & make directories
test -d _build || mkdir _build
test -d _install || mkdir _install

# Move to build folder
cd _build

# Run CMake Configurations
cmake .. -DCMAKE_INSTALL_PREFIX=../_install -DSOAP=1 -DPLAYERBOTS=1 -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl/

# Check OpenSSL
which openssl

# Compile the Project
make -j 6
