#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# EPICYKLOS tests (pruebas):
#   1. the DSP core on this machine (tests/dsp/test_engine.cpp): timing, polymetry, mute/solo, levels
#   2. the host simulator runs every script in app/tests/*.txt (the panel, the views, the sound)
#   3. the browser emulator, built and played headless (tools/headless.mjs): it boots and makes sound
set -e
cd "$(dirname "$0")/.."
fail=0
mkdir -p build tests/dsp/out
CXX="${CXX:-c++}"
"$CXX" -std=c++14 -O2 -Wall -Wextra -fno-exceptions -fno-rtti -DPLAT_HOST -I app/dsp -I base/include \
    tests/dsp/test_engine.cpp app/dsp/engine.cpp app/dsp/factory.cpp -o build/test_engine
./build/test_engine || fail=1
./build.sh --host
for t in app/tests/*.txt; do
    out=$(app/build/host/epicyklos_host "$t" app/build/host 2>&1) || true
    echo "$out" | tail -1
    if echo "$out" | grep -q "FAIL\|EXPECTATIONS FAILED"; then echo "$out" | grep "FAIL"; fail=1; fi
done
if command -v node >/dev/null 2>&1; then
    ./build.sh --web
    mkdir -p app/build/headless
    printf '%s' '[["wait",800],["shot","boot"],["btn",1024],["wait",60],["btn",0],["wait",2500],["shot","playing"]]' \
        > app/build/headless/script.json
    node tools/headless.mjs app/build/emu/app.wasm app/build/headless --min-peak 0.02 || fail=1
fi
[ $fail = 0 ] && echo "tests: all passed" || echo "tests: FAILED"
exit $fail
