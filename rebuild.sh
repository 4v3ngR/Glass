#!/bin/sh

SRC_DIR=$(pwd)
BUILD_DIR="$SRC_DIR/build"

cmake --build "$BUILD_DIR" && \
	cd "$BUILD_DIR" && \
	sudo cmake --install .
