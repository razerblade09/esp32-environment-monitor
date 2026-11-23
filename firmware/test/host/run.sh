#!/usr/bin/env bash
# Build and run the host-side unit tests (no ESP-IDF or hardware needed).
set -euo pipefail

cd "$(dirname "$0")"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

BME=../../components/bme280
CFLAGS=(-std=c11 -Wall -Wextra -Werror -I"$BME/include")

# Pure maths and calibration decoding
gcc "${CFLAGS[@]}" test_bme280_compensate.c "$BME/bme280_compensate.c" -lm \
    -o "$OUT/test_bme280_compensate"
"$OUT/test_bme280_compensate"

# Driver logic against a fake I2C device (ESP-IDF headers are stubbed in stubs/)
gcc "${CFLAGS[@]}" -Istubs test_bme280_driver.c "$BME/bme280.c" "$BME/bme280_compensate.c" -lm \
    -o "$OUT/test_bme280_driver"
"$OUT/test_bme280_driver"
