#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Download the WASI sysroot and compiler builtins for tools/wasi-emcc (descarga el sysroot de WASI).
set -e
D="$(dirname "$0")/wasi-emcc/.cache"
mkdir -p "$D"
U=https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-25
[ -d "$D/wasi-sysroot-25.0" ] || curl -fsSL "$U/wasi-sysroot-25.0.tar.gz" | tar -xz -C "$D"
[ -d "$D/libclang_rt.builtins-wasm32-wasi-25.0" ] || curl -fsSL "$U/libclang_rt.builtins-wasm32-wasi-25.0.tar.gz" | tar -xz -C "$D"
echo "wasi: $D"
