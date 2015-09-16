#!/usr/bin/env bash

set -eu

if [[ x$OSTYPE =~ ^xdarwin ]]; then
  brew install "mysql"
fi