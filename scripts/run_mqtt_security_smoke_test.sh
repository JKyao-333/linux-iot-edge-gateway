#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

BUILD_DIR="${BUILD_DIR:-$HOME/linux-iot-edge-gateway-build}"
MQTT_TEST_BIN="${BUILD_DIR}/mqtt_client_test"
RUNTIME_DIR="$(mktemp -d /tmp/iot-mqtt-security.XXXXXX)"

USERNAME="gateway-test"
PASSWORD="temporary-test-password"
TOPIC="sensor/10/data"
BROKER_HOST="127.0.0.1"
BROKER_PID=""
SUBSCRIBER_PID=""

cleanup() {
    for pid in "${SUBSCRIBER_PID}" "${BROKER_PID}"; do
        if [[ -n "${pid}" ]]; then
            kill "${pid}" 2>/dev/null || true
            wait "${pid}" 2>/dev/null || true
        fi
    done

    rm -rf -- "${RUNTIME_DIR}"
}

trap cleanup EXIT INT TERM

if [[ ! -x "${MQTT_TEST_BIN}" ]]; then
    echo "[ERROR] MQTT test executable not found: ${MQTT_TEST_BIN}"
    exit 1
fi

PORT="$(python3 - <<'PY'
import socket

with socket.socket() as sock:
    sock.bind(("127.0.0.1", 0))
    print(sock.getsockname()[1])
PY
)"

echo "[INFO] generating temporary CA and server certificate"

openssl req \
    -x509 \
    -newkey rsa:2048 \
    -nodes \
    -days 1 \
    -subj "/CN=IoT Gateway Test CA" \
    -keyout "${RUNTIME_DIR}/ca.key" \
    -out "${RUNTIME_DIR}/ca.crt" \
    >/dev/null 2>&1

openssl req \
    -newkey rsa:2048 \
    -nodes \
    -subj "/CN=localhost" \
    -keyout "${RUNTIME_DIR}/server.key" \
    -out "${RUNTIME_DIR}/server.csr" \
    >/dev/null 2>&1

cat >"${RUNTIME_DIR}/server.ext" <<'EOF'
subjectAltName=DNS:localhost,IP:127.0.0.1
extendedKeyUsage=serverAuth
EOF

openssl x509 \
    -req \
    -days 1 \
    -sha256 \
    -in "${RUNTIME_DIR}/server.csr" \
    -CA "${RUNTIME_DIR}/ca.crt" \
    -CAkey "${RUNTIME_DIR}/ca.key" \
    -CAcreateserial \
    -extfile "${RUNTIME_DIR}/server.ext" \
    -out "${RUNTIME_DIR}/server.crt" \
    >/dev/null 2>&1

mosquitto_passwd \
    -b \
    -c "${RUNTIME_DIR}/passwords" \
    "${USERNAME}" \
    "${PASSWORD}"

cat >"${RUNTIME_DIR}/mosquitto.conf" <<EOF
listener ${PORT} ${BROKER_HOST}
allow_anonymous false
password_file ${RUNTIME_DIR}/passwords
cafile ${RUNTIME_DIR}/ca.crt
certfile ${RUNTIME_DIR}/server.crt
keyfile ${RUNTIME_DIR}/server.key
EOF

echo "[INFO] starting authenticated TLS Mosquitto on port ${PORT}"

mosquitto \
    -c "${RUNTIME_DIR}/mosquitto.conf" \
    >"${RUNTIME_DIR}/broker.log" 2>&1 &
BROKER_PID=$!

for _ in {1..50}; do
    if ss -lnt | grep -q ":${PORT} "; then
        break
    fi

    if ! kill -0 "${BROKER_PID}" 2>/dev/null; then
        echo "[ERROR] secure Mosquitto exited during startup"
        cat "${RUNTIME_DIR}/broker.log"
        exit 1
    fi

    sleep 0.1
done

if ! ss -lnt | grep -q ":${PORT} "; then
    echo "[ERROR] secure Mosquitto did not start"
    cat "${RUNTIME_DIR}/broker.log"
    exit 1
fi

mosquitto_sub \
    -h "${BROKER_HOST}" \
    -p "${PORT}" \
    --cafile "${RUNTIME_DIR}/ca.crt" \
    -u "${USERNAME}" \
    -P "${PASSWORD}" \
    -t "${TOPIC}" \
    -C 1 \
    -W 10 \
    >"${RUNTIME_DIR}/subscriber.out" &
SUBSCRIBER_PID=$!

sleep 0.2

echo "[INFO] publishing with C++ MQTT TLS client"

if ! "${MQTT_TEST_BIN}" \
    "${BROKER_HOST}" \
    "${PORT}" \
    "${USERNAME}" \
    "${PASSWORD}" \
    "${RUNTIME_DIR}/ca.crt"; then

    echo "[ERROR] C++ MQTT TLS client failed"
    cat "${RUNTIME_DIR}/broker.log"
    exit 1
fi

if ! wait "${SUBSCRIBER_PID}"; then
    SUBSCRIBER_PID=""
    echo "[ERROR] secure MQTT subscriber received no message"
    cat "${RUNTIME_DIR}/broker.log"
    exit 1
fi

SUBSCRIBER_PID=""

if ! grep -Fq '"device_id":16' \
    "${RUNTIME_DIR}/subscriber.out"; then

    echo "[ERROR] secure MQTT payload is incorrect"
    cat "${RUNTIME_DIR}/subscriber.out"
    exit 1
fi

echo "[PASS] MQTT authentication and TLS smoke test passed"
