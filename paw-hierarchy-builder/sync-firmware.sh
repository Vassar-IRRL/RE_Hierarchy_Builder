#!/usr/bin/env bash
#
# Copy the ethology firmware from the main PAW-Robotics tree into this repo,
# and check that the manifest in index.html still matches what is on disk.
#
#   ./sync-firmware.sh ~/src/PAW-Robotics
#
# Run this after ANY change to firmware/shared/ in the main project. The web
# repo ships its own copy of the sources with every download, so a stale copy
# means students get sketches built against old firmware — and those compile
# cleanly, so nothing warns you.
#
# Direction of travel:
#   arduino/ethology_ble_robot/  ->  firmware/ethology/   (the 14 Cog/Robot files)
#
# CogBluetooth and CogDisplay are deliberately excluded: the downloaded student
# sketch installs a hierarchy from a const array and has no BLE link and no
# display shield, so bundling them would add ArduinoBLE and
# Arduino_GigaDisplay_GFX as hard dependencies of every download.

set -euo pipefail

SRC_ROOT="${1:-}"
if [[ -z "$SRC_ROOT" ]]; then
    echo "usage: $0 <path-to-PAW-Robotics>" >&2
    exit 2
fi

# Accept either the main tree's firmware/shared or an Arduino sketch folder
# (this repo's arduino/ethology_ble_robot, or a copy of it).
if   [[ -d "$SRC_ROOT/firmware/shared"           ]]; then SRC="$SRC_ROOT/firmware/shared"
elif [[ -d "$SRC_ROOT/ethology_ble_robot"        ]]; then SRC="$SRC_ROOT/ethology_ble_robot"
elif [[ -f "$SRC_ROOT/EthologyRobot.h"           ]]; then SRC="$SRC_ROOT"
else SRC="$SRC_ROOT/firmware/shared"; fi
DEST="$(cd "$(dirname "$0")" && pwd)/firmware/ethology"

[[ -d "$SRC" ]] || { echo "no such directory: $SRC" >&2; exit 1; }
mkdir -p "$DEST"

# The dependency closure of EthologyRobot.h. Deliberately excludes
# CogPotentialField and CogVisLight, which nothing in the include graph
# reaches, and CogHUD, which would drag Arduino_GigaDisplay_GFX into every
# student sketch.
CLASSES=(EthologyRobot Robot CogServo CogAnaDigi CogProximity CogLight CogCollision)

changed=0
for cls in "${CLASSES[@]}"; do
    for ext in h cpp; do
        f="$cls.$ext"
        [[ -f "$SRC/$f" ]] || { echo "MISSING in source: $f" >&2; exit 1; }
        if [[ ! -f "$DEST/$f" ]] || ! cmp -s "$SRC/$f" "$DEST/$f"; then
            cp "$SRC/$f" "$DEST/$f"
            echo "  updated  $f"
            changed=$((changed + 1))
        fi
    done
done

# CogDisplay ships from firmware/display/, bundled only by the display profile.
DISP="$(dirname "$DEST")/display"
mkdir -p "$DISP"
for f in CogDisplay.h CogDisplay.cpp; do
    if [[ -f "$SRC/$f" ]]; then
        if [[ ! -f "$DISP/$f" ]] || ! cmp -s "$SRC/$f" "$DISP/$f"; then
            cp "$SRC/$f" "$DISP/$f"
            echo "  updated  display/$f"
            changed=$((changed + 1))
        fi
    else
        echo "  WARNING: $f not found in source — firmware/display/ may be stale" >&2
    fi
done

[[ $changed -eq 0 ]] && echo "  already current"

# Cross-check: every file on disk must be listed in FIRMWARE_FILES, and vice
# versa. A file present but unlisted is silently left out of every download.
echo
echo "checking FIRMWARE_FILES manifest..."

INDEX="$(dirname "$DEST")/../index.html"
listed=$(sed -n '/const FIRMWARE_FILES = \[/,/\];/p' "$INDEX" \
         | grep -o '"[A-Za-z]*\.\(h\|cpp\)"' | tr -d '"' | sort)
ondisk=$(ls "$DEST" | sort)

if [[ "$listed" == "$ondisk" ]]; then
    echo "  manifest matches ($(echo "$ondisk" | wc -l | tr -d ' ') files)"
else
    echo "  MISMATCH — index.html and firmware/ethology/ disagree:" >&2
    diff <(echo "$listed") <(echo "$ondisk") \
        | sed 's/^</  only in manifest: /; s/^>/  only on disk:     /' >&2
    exit 1
fi

echo
echo "Note: CogBluetooth is never bundled. CogDisplay ships in firmware/display/"
echo "and is added to a download only by the 'Giga + Display' board profile."
