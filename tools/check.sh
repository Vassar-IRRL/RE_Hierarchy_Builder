#!/usr/bin/env bash
#
# Verify the firmware and the web app without an Arduino IDE or a robot.
#
#   ./tools/check.sh
#
# What it catches that a syntax check does not: CogDisplay.cpp is compiled
# separately from the sketch, so a build switch defined in the wrong place
# produces a LINK error, not a compile error. That failure is invisible to
# `g++ -fsyntax-only` on individual files. This links a real two-translation-
# unit build at every display setting.
#
# The stub headers under tools/stubs/ stand in for the Arduino core, Servo and
# Arduino_GigaDisplay_GFX. They declare only what this firmware uses. If a
# check fails with "no member named X", the stub is behind the real API —
# add the signature, do not work around it in the firmware.
#
# Not a substitute for compiling on hardware: it cannot catch board-specific
# behaviour, timing, or anything about the servos. It catches the mistakes
# that are expensive to find on a bench.

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STUBS="$ROOT/tools/stubs"
SKETCH="$ROOT/arduino/ethology_ble_robot"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

fail=0
pass() { printf '  \033[32m✓\033[0m %s\n' "$1"; }
bad()  { printf '  \033[31m✗\033[0m %s\n' "$1"; fail=1; }

# ── 1. Firmware links at every display setting ───────────────────────────────
# CogBluetooth is excluded: it needs ArduinoBLE, which is not stubbed. The
# classes below are the ones the student download ships, plus CogDisplay.
echo "Firmware link check"

CLASSES=(EthologyRobot Robot CogServo CogAnaDigi CogProximity CogLight CogCollision CogDisplay)

cp "$STUBS"/*.h "$STUBS"/stubs.cpp "$STUBS"/harness.cpp "$WORK/"
cp "$SKETCH"/PAWConfig.h "$WORK/"
for c in "${CLASSES[@]}"; do
    cp "$SKETCH/$c.h" "$SKETCH/$c.cpp" "$WORK/" 2>/dev/null || {
        bad "missing source: $c"; continue; }
done

SRCS=(harness.cpp stubs.cpp)
for c in "${CLASSES[@]}"; do SRCS+=("$c.cpp"); done

for cfg in "0 0" "1 0" "1 1"; do
    set -- $cfg
    sed -e "s/^#define PAW_USE_DISPLAY .*/#define PAW_USE_DISPLAY $1/" \
        -e "s/^#define PAW_DISPLAY_DEV .*/#define PAW_DISPLAY_DEV $2/" \
        "$SKETCH/PAWConfig.h" > "$WORK/PAWConfig.h"

    if (cd "$WORK" && g++ -std=gnu++17 -Wall -I. "${SRCS[@]}" -o /dev/null 2>err.txt); then
        pass "USE_DISPLAY=$1 DISPLAY_DEV=$2"
    else
        bad "USE_DISPLAY=$1 DISPLAY_DEV=$2"
        grep -m5 -E 'undefined reference|error:' "$WORK/err.txt" | sed 's/^/      /'
    fi
done

# ── 2. The web app's script parses ───────────────────────────────────────────
echo "Web app"
if command -v node >/dev/null 2>&1; then
    if node -e '
        const fs = require("fs");
        const html = fs.readFileSync(process.argv[1], "utf8");
        const js = html.split("<script>")[1].split("</scr"+"ipt>")[0];
        new Function(js);
    ' "$ROOT/index.html" 2>"$WORK/js.txt"; then
        pass "index.html script parses"
    else
        bad "index.html script"
        sed 's/^/      /' "$WORK/js.txt" | head -5
    fi
else
    printf '  - node not installed; skipping the script parse check\n'
fi

# ── 3. The bundled copy matches the sketch ───────────────────────────────────
echo "Firmware sync"
if bash "$ROOT/sync-firmware.sh" "$ROOT/arduino" >"$WORK/sync.txt" 2>&1; then
    pass "firmware/ethology matches arduino/, manifest agrees"
else
    bad "sync-firmware.sh"
    sed 's/^/      /' "$WORK/sync.txt" | head -8
fi

echo
if [[ $fail -eq 0 ]]; then
    echo "All checks passed."
else
    echo "FAILURES above."
fi
exit $fail
