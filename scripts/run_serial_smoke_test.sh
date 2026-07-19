#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

BUILD_DIR="${BUILD_DIR:-$HOME/linux-iot-edge-gateway-build}"
GATEWAY_BIN="${BUILD_DIR}/edge_gateway"

RUNTIME_DIR="$(
    mktemp -d /tmp/iot-gateway-smoke.XXXXXX
)"

STM32_DEVICE="${RUNTIME_DIR}/tty_stm32"
GATEWAY_DEVICE="${RUNTIME_DIR}/tty_gateway"
SOCAT_LOG="${RUNTIME_DIR}/socat.log"
GATEWAY_LOG="${RUNTIME_DIR}/gateway.log"

SOCAT_PID=""
GATEWAY_PID=""

cleanup() {
    if [[ -n "${GATEWAY_PID}" ]]; then
        kill "${GATEWAY_PID}" 2>/dev/null || true
        wait "${GATEWAY_PID}" 2>/dev/null || true
    fi

    if [[ -n "${SOCAT_PID}" ]]; then
        kill "${SOCAT_PID}" 2>/dev/null || true
        wait "${SOCAT_PID}" 2>/dev/null || true
    fi

    rm -rf -- "${RUNTIME_DIR}"
}

trap cleanup EXIT INT TERM

if [[ ! -x "${GATEWAY_BIN}" ]]; then
    echo "[ERROR] gateway executable not found: ${GATEWAY_BIN}"
    exit 1
fi

if ! ss -lnt | grep -q ':1883'; then
    echo "[ERROR] Mosquitto is not listening on port 1883"
    exit 1
fi

echo "[INFO] starting virtual serial pair"

socat -d -d \
    "pty,raw,echo=0,link=${STM32_DEVICE}" \
    "pty,raw,echo=0,link=${GATEWAY_DEVICE}" \
    >"${SOCAT_LOG}" 2>&1 &

SOCAT_PID=$!

for _ in {1..20}; do
    if [[ -e "${STM32_DEVICE}" && -e "${GATEWAY_DEVICE}" ]]; then
        break
    fi

    sleep 0.1
done

if [[ ! -e "${STM32_DEVICE}" || ! -e "${GATEWAY_DEVICE}" ]]; then
    echo "[ERROR] virtual serial pair was not created"
    cat "${SOCAT_LOG}"
    exit 1
fi

echo "[INFO] starting edge gateway"

(
    cd "${RUNTIME_DIR}"
    "${GATEWAY_BIN}" \
        "${PROJECT_DIR}/config/gateway.yaml" \
        "${GATEWAY_DEVICE}"
) >"${GATEWAY_LOG}" 2>&1 &

GATEWAY_PID=$!

for _ in {1..20}; do
    if grep -Fq "serial opened" "${GATEWAY_LOG}"; then
        break
    fi

    sleep 0.1
done

if ! grep -Fq "serial opened" "${GATEWAY_LOG}"; then
    echo "[ERROR] gateway did not open the serial port"
    cat "${GATEWAY_LOG}"
    exit 1
fi
for _ in {1..30}; do
    if grep -Fq "MQTT connected" "${GATEWAY_LOG}"; then
        break
    fi

    sleep 0.1
done

if ! grep -Fq "MQTT connected" "${GATEWAY_LOG}"; then
    echo "[ERROR] gateway did not connect to MQTT"
    cat "${GATEWAY_LOG}"
    exit 1
fi
echo "[INFO] sending one frame in two parts"

python3 \
    "${PROJECT_DIR}/scripts/inject_stream_cases.py" \
    "${STM32_DEVICE}"

sleep 1

if ! grep -Fq "[sensor] parsed data" "${GATEWAY_LOG}"; then
    echo "[ERROR] sensor frame was not parsed"
    cat "${GATEWAY_LOG}"
    exit 1
fi

if ! grep -Fq "MQTT publish acknowledged" "${GATEWAY_LOG}"; then
    echo "[ERROR] MQTT message was not published"
    cat "${GATEWAY_LOG}"
    exit 1
fi

echo "[PASS] serial-to-MQTT smoke test passed"
