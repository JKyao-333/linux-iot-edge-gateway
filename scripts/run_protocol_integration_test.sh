#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${HOME}/linux-iot-edge-gateway-build}"
CAN_INTERFACE="${CAN_INTERFACE:-vcan0}"
RUNTIME_DIR="$(mktemp -d /tmp/iot-protocol-test.XXXXXX)"
UART_SOURCE="${RUNTIME_DIR}/uart_source"
UART_GATEWAY="${RUNTIME_DIR}/uart_gateway"
MODBUS_GATEWAY="${RUNTIME_DIR}/modbus_gateway"
MODBUS_SLAVE="${RUNTIME_DIR}/modbus_slave"
CONFIG="${RUNTIME_DIR}/gateway.yaml"
GATEWAY_LOG="${RUNTIME_DIR}/gateway.log"
MQTT_LOG="${RUNTIME_DIR}/mqtt.log"

PIDS=()
cleanup() {
    for pid in "${PIDS[@]:-}"; do
        kill "${pid}" 2>/dev/null || true
        wait "${pid}" 2>/dev/null || true
    done
    rm -rf -- "${RUNTIME_DIR}"
}
trap cleanup EXIT INT TERM

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[FAIL] required command not found: $1"
        exit 1
    fi
}

for command in cmake socat python3 mosquitto_sub ss ip; do
    require_command "${command}"
done

python3 -c "import serial" 2>/dev/null || {
    echo "[FAIL] Python module pyserial is required"
    exit 1
}

if ! ss -lnt | grep -q ':1883'; then
    echo "[FAIL] Mosquitto must be listening on port 1883"
    exit 1
fi

if ! ip link show "${CAN_INTERFACE}" >/dev/null 2>&1; then
    if [[ "${EUID}" -eq 0 ]]; then
        modprobe vcan 2>/dev/null || true
        ip link add dev "${CAN_INTERFACE}" type vcan
        ip link set up "${CAN_INTERFACE}"
    elif sudo -n true >/dev/null 2>&1; then
        sudo -n modprobe vcan 2>/dev/null || true
        sudo -n ip link add dev "${CAN_INTERFACE}" type vcan
        sudo -n ip link set up "${CAN_INTERFACE}"
    else
        echo "[FAIL] ${CAN_INTERFACE} is absent and CAP_NET_ADMIN is unavailable"
        echo "[INFO] create it first: sudo modprobe vcan && sudo ip link add dev ${CAN_INTERFACE} type vcan && sudo ip link set up ${CAN_INTERFACE}"
        exit 1
    fi
fi

cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" --target edge_gateway --parallel

socat "pty,raw,echo=0,link=${UART_SOURCE}" "pty,raw,echo=0,link=${UART_GATEWAY}" \
    >"${RUNTIME_DIR}/uart_socat.log" 2>&1 &
PIDS+=("$!")
socat "pty,raw,echo=0,link=${MODBUS_GATEWAY}" "pty,raw,echo=0,link=${MODBUS_SLAVE}" \
    >"${RUNTIME_DIR}/modbus_socat.log" 2>&1 &
PIDS+=("$!")

for _ in {1..30}; do
    [[ -e "${UART_SOURCE}" && -e "${UART_GATEWAY}" \
        && -e "${MODBUS_GATEWAY}" && -e "${MODBUS_SLAVE}" ]] && break
    sleep 0.1
done

if [[ ! -e "${UART_SOURCE}" || ! -e "${UART_GATEWAY}" \
    || ! -e "${MODBUS_GATEWAY}" || ! -e "${MODBUS_SLAVE}" ]]; then
    echo "[FAIL] virtual serial endpoints were not created"
    cat "${RUNTIME_DIR}/uart_socat.log" "${RUNTIME_DIR}/modbus_socat.log" 2>/dev/null || true
    exit 1
fi

python3 "${PROJECT_DIR}/scripts/mock_modbus_rtu_slave.py" "${MODBUS_SLAVE}" \
    >"${RUNTIME_DIR}/modbus.log" 2>&1 &
PIDS+=("$!")
sleep 0.2
if ! kill -0 "${PIDS[-1]}" 2>/dev/null; then
    echo "[FAIL] Modbus RTU mock slave did not start"
    cat "${RUNTIME_DIR}/modbus.log" 2>/dev/null || true
    exit 1
fi

cat >"${CONFIG}" <<EOF
serial:
  devices: [${UART_GATEWAY}]
  baud_rate: 115200
  reconnect_interval_seconds: 1
modbus:
  enabled: true
  port: ${MODBUS_GATEWAY}
  baud_rate: 115200
  slave_id: 1
  function_code: 3
  start_address: 0
  register_count: 6
  poll_interval_ms: 200
  response_timeout_ms: 150
can:
  enabled: true
  interface: ${CAN_INTERFACE}
  heartbeat_timeout_seconds: 5
mqtt:
  host: localhost
  port: 1883
  topic_prefix: sensor
  cache_retry_interval_seconds: 1
tcp:
  enabled: false
  host: 127.0.0.1
  port: 9000
http:
  enabled: false
  host: 127.0.0.1
  port: 8080
cache:
  type: sqlite
  path: ${RUNTIME_DIR}/pending.db
log:
  path: ${RUNTIME_DIR}/gateway-file.log
  level: DEBUG
EOF

mosquitto_sub -h localhost -p 1883 -t 'sensor/+/data' -v >"${MQTT_LOG}" 2>&1 &
PIDS+=("$!")
"${BUILD_DIR}/edge_gateway" "${CONFIG}" >"${GATEWAY_LOG}" 2>&1 &
PIDS+=("$!")
sleep 1

python3 "${PROJECT_DIR}/scripts/inject_stream_cases.py" "${UART_SOURCE}" >/dev/null
python3 - "${CAN_INTERFACE}" <<'PY'
import socket
import struct
import sys

frame = struct.pack("=IB3x8s", 0x123, 8, bytes.fromhex("00FD0260012C2500"))
sock = socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
sock.bind((sys.argv[1],))
sock.send(frame)
sock.close()
PY

for _ in {1..50}; do
    if grep -q '^sensor/16/data ' "${MQTT_LOG}" \
        && grep -q '^sensor/1/data ' "${MQTT_LOG}" \
        && grep -q '^sensor/35/data ' "${MQTT_LOG}"; then
        echo "[PASS] UART -> SensorData -> MQTT"
        echo "[PASS] Modbus RTU -> SensorData -> MQTT"
        echo "[PASS] SocketCAN -> SensorData -> MQTT"
        echo "[PASS] protocol integration test passed"
        exit 0
    fi
    sleep 0.1
done

echo "[FAIL] expected protocol topics were not all received"
cat "${MQTT_LOG}"
echo "--- gateway log ---"
cat "${GATEWAY_LOG}"
exit 1
