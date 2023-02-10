#!/bin/bash

set -e

# Install Required Packages
sudo apt-get install -y \
  build-essential \
  clang \
  cmake \
  default-libmysqlclient-dev \
  git \
  libssl-dev \
  libbz2-dev \
  libace-dev \
  make \
;
