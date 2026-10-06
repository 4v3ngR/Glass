#!/usr/bin/env bash
set -euo pipefail

SRC_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

if (( EUID != 0 )); then
    exec sudo -- "$0" "$@"
fi

"$SRC_DIR/install.sh" remove
