#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build EPICYKLOS (compila EPICYKLOS). Arguments go to base/tools/build.py:
#   ./build.sh              firmware for the FM-1 (JieLi toolchain + AC79 SDK) -> app/build/epicyklos.fwsc
#   ./build.sh --host       host simulator                                     -> app/build/host/epicyklos_host
#   ./build.sh --web        browser emulator (emcc, or tools/wasi-emcc)        -> app/build/emu/
#   ./build.sh --release X.Y
set -e
cd "$(dirname "$0")"
if [ "$1" = "--web" ] && ! command -v emcc >/dev/null 2>&1; then
    sh tools/get_wasi.sh >/dev/null
    PATH="$PWD/tools/wasi-emcc:$PATH"; export PATH
fi
exec python3 base/tools/build.py app "$@"
