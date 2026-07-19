#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

BUILD_DIR="${BUILD_DIR:-$HOME/linux-iot-edge-gateway-build}"
GATEWAY_BIN="${BUILD_DIR}/edge_gateway"
RUNTIME_DIR="$(
    mktemp -d /tmp/iot-gateway-shutdown.XXXXXX
)"

STM32_DEVICE="${RUNTIME_DIR}/tty_stm32"
GATEWAY_DEVICE="${RUNTIME_DIR}/tty_gateway"
CONFIG_PATH="${RUNTIME_DIR}/gateway.yaml"
CONSOLE_LOG="${RUNTIME_DIR}/console.log"
SOCAT_LOG="${RUNTIME_DIR}/socat.log"

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

cat >"${CONFIG_PATH}" <<EOF
serial:
  device: ${GATEWAY_DEVICE}
  baud_rate: 115200
  reconnect_interval_seconds: 2
mqtt:
  host: localhost
  port: 1883
  topic_prefix: sensor
  cache_retry_interval_seconds: 5
tcp:
  enabled: false
  host: 127.0.0.1
  port: 9000
cache:
  path: ${RUNTIME_DIR}/pending.cache
log:
  path: ${RUNTIME_DIR}/gateway.log
  level: DEBUG
EOF

echo "[INFO] starting virtual serial pair"

socat -d -d \
    "pty,raw,echo=0,link=${STM32_DEVICE}" \
    "pty,raw,echo=0,link=${GATEWAY_DEVICE}" \
    >"${SOCAT_LOG}" 2>&1 &

SOCAT_PID=$!

for _ in {1..20}; do
    if [[ -e "${STM32_DEVICE}" \
          && -e "${GATEWAY_DEVICE}" ]]; then
        break
    fi

    sleep 0.1
done

if [[ ! -e "${STM32_DEVICE}" \
      || ! -e "${GATEWAY_DEVICE}" ]]; then
    echo "[ERROR] virtual serial pair was not created"
    cat "${SOCAT_LOG}"
    exit 1
fi

echo "[INFO] starting edge gateway"

"${GATEWAY_BIN}" "${CONFIG_PATH}" \
    >"${CONSOLE_LOG}" 2>&1 &

GATEWAY_PID=$!

for _ in {1..30}; do
    if grep -Fq "waiting for raw bytes" \
        "${CONSOLE_LOG}"; then
        break
    fi

    sleep 0.1
done

if ! grep -Fq "waiting for raw bytes" \
    "${CONSOLE_LOG}"; then
    echo "[ERROR] gateway did not enter the read loop"
    cat "${CONSOLE_LOG}"
    exit 1
fi

echo "[INFO] sending SIGTERM"
kill -TERM "${GATEWAY_PID}"

for _ in {1..50}; do
    if ! kill -0 "${GATEWAY_PID}" 2>/dev/null; then
        break
    fi

    sleep 0.1
done

if kill -0 "${GATEWAY_PID}" 2>/dev/null; then
    echo "[ERROR] gateway did not stop within 5 seconds"
    cat "${CONSOLE_LOG}"
    exit 1
fi

set +e
wait "${GATEWAY_PID}"
EXIT_CODE=$?
set -e
GATEWAY_PID=""

if [[ "${EXIT_CODE}" -ne 0 ]]; then
    echo "[ERROR] gateway exit code: ${EXIT_CODE}"
    cat "${CONSOLE_LOG}"
    exit 1
fi

for expected_log in \
    "shutdown requested" \
    "edge gateway stopped"
do
    if ! grep -Fq "${expected_log}" \
        "${CONSOLE_LOG}"; then
        echo "[ERROR] missing log: ${expected_log}"
        cat "${CONSOLE_LOG}"
        exit 1
    fi
done

echo "[PASS] graceful shutdown smoke test passed"
