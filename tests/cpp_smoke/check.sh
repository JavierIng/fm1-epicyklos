#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Compile tests/cpp_smoke/smoke.cpp with JieLi's clang for pi32v2 (the FM-1's CPU) and check the object.
#   JIELI_TOOLCHAIN (default ~/.jieli/toolchain): the directory with pi32v2/ and common/
set -e
TC="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
D="$(dirname "$0")"
OUT="${OUT:-$D/out}"
mkdir -p "$OUT"
CC="$TC/pi32v2/bin/clang"
"$CC" --version | head -1
FLAGS="-target pi32v2 -mcpu=r3 -mfprev1 -ffp-contract=off -Os -ffunction-sections -fno-builtin"
CXX="-x c++ -std=c++14 -ffreestanding -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit"
"$CC" $FLAGS $CXX -c "$D/smoke.cpp" -o "$OUT/smoke.o"
echo "compiled: $(wc -c < "$OUT/smoke.o") B"
NM=""
for t in "$TC/pi32v2/bin/llvm-nm" "$TC/common/bin/llvm-nm" "$TC/pi32v2/bin/nm" "$TC/common/bin/nm" llvm-nm nm; do
    if command -v "$t" >/dev/null 2>&1 || [ -x "$t" ]; then NM="$t"; break; fi
done
OBJDUMP=""
for t in "$TC/common/bin/objdump" "$TC/pi32v2/bin/llvm-objdump" llvm-objdump; do
    if command -v "$t" >/dev/null 2>&1 || [ -x "$t" ]; then OBJDUMP="$t"; break; fi
done
echo "nm: $NM  objdump: $OBJDUMP"
"$NM" "$OUT/smoke.o" | tee "$OUT/symbols.txt"
bad=0
if grep -E " U (__cxa_|_Zn|_Zd|__gxx|_Unwind|__dynamic_cast)" "$OUT/symbols.txt"; then echo "FAIL: needs the C++ runtime"; bad=1; fi
"$OBJDUMP" -h "$OUT/smoke.o" > "$OUT/sections.txt" 2>&1 || true
cat "$OUT/sections.txt"
if grep -E "\.init_array|\.ctors" "$OUT/sections.txt"; then echo "FAIL: global constructors"; bad=1; fi
for s in epk_smoke_init epk_smoke_render epk_smoke_twopi; do
    grep -q " T $s" "$OUT/symbols.txt" || { echo "FAIL: $s missing"; bad=1; }
done
[ $bad = 0 ] && echo "C++ smoke: OK (compiles, no runtime, no global constructors)"
exit $bad
