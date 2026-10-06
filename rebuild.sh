#!/bin/sh
set -eu

SRC_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR="$SRC_DIR/build"
JOBS=${JOBS:-2}

cmake --build "$BUILD_DIR" -j "$JOBS"
sudo cmake --install "$BUILD_DIR"
"$SRC_DIR/tools/glass-user-helper" install
