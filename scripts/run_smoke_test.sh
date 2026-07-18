#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

BUILD_DIR="${BUILD_DIR:-$HOME/linux-iot-edge-gateway-build}"

echo "[INFO] project: ${PROJECT_DIR}"
echo "[INFO] build: ${BUILD_DIR}"

if ! ss -lnt | grep -q ':1883'; then
    echo "[ERROR] Mosquitto is not listening on port 1883"
    echo "[INFO] Run: sudo service mosquitto start"
    exit 1
fi

echo "[INFO] configuring project"
cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}"

echo "[INFO] building project"
cmake --build "${BUILD_DIR}" --parallel

echo "[INFO] running CTest"
ctest \
    --test-dir "${BUILD_DIR}" \
    --output-on-failure
echo "[INFO] running serial-to-MQTT smoke test"

"${PROJECT_DIR}/scripts/run_serial_smoke_test.sh"
echo "[PASS] all smoke tests passed"
