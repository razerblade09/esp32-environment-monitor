#!/usr/bin/env bash
# Upload dist/esp32_environment_monitor.bin (from ./build.sh) to the ESP32.
#
# Usage: ./flash.sh [serial-port]     (default: $ESPPORT or /dev/ttyUSB0)
#
# Needs esptool (included in an ESP-IDF environment, or `pip install esptool`).
# Put the board in download mode first: hold BOOT, tap RESET, release BOOT.
set -euo pipefail

cd "$(dirname "$0")"

IMAGE="dist/esp32_environment_monitor.bin"
PORT="${1:-${ESPPORT:-/dev/ttyUSB0}}"

if [ ! -f "$IMAGE" ]; then
    echo "error: $IMAGE not found. Run ./build.sh first." >&2
    exit 1
fi

python -m esptool --chip esp32 --port "$PORT" --baud 460800 write_flash 0x0 "$IMAGE"
