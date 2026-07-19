#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

BUILD_DIR="${BUILD_DIR:-$HOME/linux-iot-edge-gateway-build}"
GATEWAY_BIN="${BUILD_DIR}/edge_gateway"
RUNTIME_DIR="$(mktemp -d /tmp/iot-multi-serial.XXXXXX)"

TOPIC_PREFIX="multi-serial-${$}"
CONFIG_PATH="${RUNTIME_DIR}/gateway.yaml"
GATEWAY_LOG="${RUNTIME_DIR}/gateway.log"
MQTT_OUTPUT="${RUNTIME_DIR}/mqtt.out"

STM32_DEVICE_1="${RUNTIME_DIR}/tty_stm32_1"
GATEWAY_DEVICE_1="${RUNTIME_DIR}/tty_gateway_1"
STM32_DEVICE_2="${RUNTIME_DIR}/tty_stm32_2"
GATEWAY_DEVICE_2="${RUNTIME_DIR}/tty_gateway_2"

SOCAT_PID_1=""
SOCAT_PID_2=""
GATEWAY_PID=""
SUBSCRIBER_PID=""

cleanup() {
    for pid in \
        "${SUBSCRIBER_PID}" \
        "${GATEWAY_PID}" \
        "${SOCAT_PID_1}" \
        "${SOCAT_PID_2}"; do

        if [[ -n "${pid}" ]]; then
            kill "${pid}" 2>/dev/null || true
            wait "${pid}" 2>/dev/null || true
        fi
    done

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

cat >"${CONFIG_PATH}" <<EOF
serial:
  devices:
    - ${GATEWAY_DEVICE_1}
    - ${GATEWAY_DEVICE_2}
  baud_rate: 115200
  reconnect_interval_seconds: 1
mqtt:
  host: localhost
  port: 1883
  topic_prefix: ${TOPIC_PREFIX}
  cache_retry_interval_seconds: 1
tcp:
  enabled: false
  host: 127.0.0.1
  port: 9000
cache:
  type: sqlite
  path: ${RUNTIME_DIR}/pending_messages.db
log:
  path: ${RUNTIME_DIR}/gateway-file.log
  level: DEBUG
EOF

echo "[INFO] starting two virtual serial pairs"

socat -d -d \
    "pty,raw,echo=0,link=${STM32_DEVICE_1}" \
    "pty,raw,echo=0,link=${GATEWAY_DEVICE_1}" \
    >"${RUNTIME_DIR}/socat-1.log" 2>&1 &
SOCAT_PID_1=$!

socat -d -d \
    "pty,raw,echo=0,link=${STM32_DEVICE_2}" \
    "pty,raw,echo=0,link=${GATEWAY_DEVICE_2}" \
    >"${RUNTIME_DIR}/socat-2.log" 2>&1 &
SOCAT_PID_2=$!

for _ in {1..30}; do
    if [[ -e "${STM32_DEVICE_1}"
          && -e "${GATEWAY_DEVICE_1}"
          && -e "${STM32_DEVICE_2}"
          && -e "${GATEWAY_DEVICE_2}" ]]; then

        break
    fi

    sleep 0.1
done

if [[ ! -e "${GATEWAY_DEVICE_1}" ]] \
   || [[ ! -e "${GATEWAY_DEVICE_2}" ]]; then

    echo "[ERROR] virtual serial pairs were not created"
    exit 1
fi

echo "[INFO] starting two serial workers"
"${GATEWAY_BIN}" "${CONFIG_PATH}" \
    >"${GATEWAY_LOG}" 2>&1 &
GATEWAY_PID=$!

for _ in {1..50}; do
    if grep -Fq "worker count=2" "${GATEWAY_LOG}" \
       && [[ "$(grep -Fc 'serial opened' "${GATEWAY_LOG}")" -ge 2 ]]; then

        break
    fi

    sleep 0.1
done

if ! grep -Fq "worker count=2" "${GATEWAY_LOG}" \
   || [[ "$(grep -Fc 'serial opened' "${GATEWAY_LOG}")" -lt 2 ]]; then

    echo "[ERROR] gateway did not start two serial workers"
    cat "${GATEWAY_LOG}"
    exit 1
fi

mosquitto_sub \
    -h localhost \
    -p 1883 \
    -t "${TOPIC_PREFIX}/+/data" \
    -v \
    -C 2 \
    -W 10 \
    >"${MQTT_OUTPUT}" &
SUBSCRIBER_PID=$!

sleep 0.2

echo "[INFO] sending frames through both serial ports"
python3 "${PROJECT_DIR}/scripts/inject_stream_cases.py" \
    "${STM32_DEVICE_1}" 0x10 &
SENDER_PID_1=$!

python3 "${PROJECT_DIR}/scripts/inject_stream_cases.py" \
    "${STM32_DEVICE_2}" 0x11 &
SENDER_PID_2=$!

wait "${SENDER_PID_1}"
wait "${SENDER_PID_2}"

if ! wait "${SUBSCRIBER_PID}"; then
    SUBSCRIBER_PID=""
    echo "[ERROR] MQTT did not receive both serial messages"
    cat "${GATEWAY_LOG}"
    exit 1
fi

SUBSCRIBER_PID=""

if ! grep -Fq "${TOPIC_PREFIX}/16/data" "${MQTT_OUTPUT}" \
   || ! grep -Fq "${TOPIC_PREFIX}/17/data" "${MQTT_OUTPUT}"; then

    echo "[ERROR] MQTT topics do not contain both device IDs"
    cat "${MQTT_OUTPUT}"
    exit 1
fi

echo "[PASS] multi-serial MQTT smoke test passed"
