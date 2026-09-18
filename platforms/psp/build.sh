#!/bin/bash
# Builds the PSP version from WSL (or any Linux): pspdev has no Windows toolchain, so the build runs
# there and writes into build/ next to this script, which Windows sees as platforms\psp\build.
#
#   wsl bash build.sh            a normal build
#   wsl bash build.sh clean      throws the build folder away first
#
# PSPDEV is where the pspdev toolchain was unpacked, C:\psp_dev unless it is set already.
set -e

export PSPDEV="${PSPDEV:-/mnt/c/psp_dev}"
export PATH="$PSPDEV/bin:$PATH"

if [ ! -x "$PSPDEV/bin/psp-gcc" ]; then
	echo "no pspdev toolchain in $PSPDEV, set PSPDEV to where it was unpacked" >&2
	exit 1
fi

here="$(cd "$(dirname "$0")" && pwd)"
cd "$here"

if [ "$1" = "clean" ]; then
	rm -rf build
fi

psp-cmake -S . -B build
cmake --build build -j "$(nproc)"

echo
ls -l build/EBOOT.PBP
