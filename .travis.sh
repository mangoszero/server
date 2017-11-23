#!/usr/bin/env bash

set -eu

if [[ x$OSTYPE =~ ^xdarwin ]]; then
  brew update
  brew install "mysql"
fi
