#!/usr/bin/env bash

set -eu

if [[ x$OSTYPE =~ ^xdarwin ]]; then
  brew update
  brew install "mysql"
  brew reinstall openssl@1.1
  cmake -DOPENSSL_ROOT_DIR=/usr/local/ssl -DOPENSSL_LIBRARIES=/usr/local/ssl/lib
fi
