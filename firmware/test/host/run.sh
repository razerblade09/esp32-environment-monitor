#!/usr/bin/env bash
# Build and run the host-side unit tests (no ESP-IDF or hardware needed).
set -euo pipefail

cd "$(dirname "$0")"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

gcc -std=c11 -Wall -Wextra -Werror \
    -I../../components/bme280/include \
    test_bme280_compensate.c ../../components/bme280/bme280_compensate.c \
    -lm -o "$OUT/test_bme280_compensate"

"$OUT/test_bme280_compensate"
