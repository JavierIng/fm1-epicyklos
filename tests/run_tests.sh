#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# EPICYKLOS tests (pruebas): the host simulator runs every script in app/tests/*.txt; then the browser
# emulator is built and played headless (tools/headless.mjs) to check that it boots and makes sound.
set -e
cd "$(dirname "$0")/.."
./build.sh --host
fail=0
for t in app/tests/*.txt; do
    case "$t" in *screens.txt) continue ;; esac
    out=$(app/build/host/epicyklos_host "$t" app/build/host 2>&1) || true
    echo "$out" | tail -1
    echo "$out" | grep -q "EXPECTATIONS FAILED" && { echo "$out" | grep FAIL; fail=1; }
done
if command -v node >/dev/null 2>&1; then
    ./build.sh --web
    mkdir -p app/build/headless
    printf '%s' '[["wait",1500],["shot","boot"],["key",1],["wait",400],["key",0],["wait",300],["shot","played"]]' \
        > app/build/headless/script.json
    node tools/headless.mjs app/build/emu/app.wasm app/build/headless --min-peak 0.01 || fail=1
fi
exit $fail
