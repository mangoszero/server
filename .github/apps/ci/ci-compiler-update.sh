#!/bin/bash

set -e

# Update to Clang Compilers
time sudo update-alternatives --install /usr/bin/cc cc /usr/bin/clang 100
time sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang 100
