#!/usr/bin/env bash
# Build the firmware and produce a single flashable image in dist/.
#
# Prerequisite: an active ESP-IDF environment (idf.py on PATH), e.g. the
# ESP-IDF terminal in VS Code, or `. $IDF_PATH/export.sh`.
#
# Output: dist/esp32_environment_monitor.bin
#   Merged image (bootloader + partition table + app) to be written at
#   offset 0x0. Use ./flash.sh to upload it.
set -euo pipefail

cd "$(dirname "$0")"

if ! command -v idf.py >/dev/null 2>&1; then
    echo "error: idf.py not found. Activate ESP-IDF first (. \$IDF_PATH/export.sh)." >&2
    exit 1
fi

idf.py build

mkdir -p dist
(cd build && python -m esptool --chip esp32 merge_bin \
    -o ../dist/esp32_environment_monitor.bin @flash_args)

echo
echo "Built: firmware/dist/esp32_environment_monitor.bin"
echo "Flash with: ./flash.sh <port>"
