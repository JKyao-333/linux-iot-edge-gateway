#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

BUILD_DIR="${BUILD_DIR:-$HOME/linux-iot-edge-gateway-build}"
TCP_TEST_BIN="${BUILD_DIR}/tcp_client_test"

RUNTIME_DIR="$(
    mktemp -d /tmp/iot-tcp-smoke.XXXXXX
)"

TCP_SERVER_LOG="${RUNTIME_DIR}/tcp_server.log"
TCP_CLIENT_LOG="${RUNTIME_DIR}/tcp_client.log"
TCP_SERVER_PID=""

cleanup() {
    if [[ -n "${TCP_SERVER_PID}" ]]; then
        kill "${TCP_SERVER_PID}" 2>/dev/null || true
        wait "${TCP_SERVER_PID}" 2>/dev/null || true
    fi

    rm -rf -- "${RUNTIME_DIR}"
}

trap cleanup EXIT INT TERM

if [[ ! -x "${TCP_TEST_BIN}" ]]; then
    echo "[ERROR] TCP test executable not found: ${TCP_TEST_BIN}"
    exit 1
fi

if ss -lnt | grep -q ':9000'; then
    echo "[ERROR] TCP port 9000 is already in use"
    exit 1
fi

echo "[INFO] starting TCP test server"

python3 -u \
    "${PROJECT_DIR}/scripts/mock_tcp_server.py" \
    9000 \
    >"${TCP_SERVER_LOG}" 2>&1 &

TCP_SERVER_PID=$!

for _ in {1..20}; do
    if grep -Fq \
        "TCP server listening" \
        "${TCP_SERVER_LOG}"; then

        break
    fi

    sleep 0.1
done

if ! grep -Fq \
    "TCP server listening" \
    "${TCP_SERVER_LOG}"; then

    echo "[ERROR] TCP test server did not start"
    cat "${TCP_SERVER_LOG}"
    exit 1
fi

echo "[INFO] running C++ TCP client"

"${TCP_TEST_BIN}" \
    >"${TCP_CLIENT_LOG}" 2>&1

sleep 0.2

cat "${TCP_CLIENT_LOG}"
cat "${TCP_SERVER_LOG}"

if ! grep -Fq \
    "TCP test passed" \
    "${TCP_CLIENT_LOG}"; then

    echo "[ERROR] C++ TCP client test failed"
    exit 1
fi

if ! grep -Fq \
    '[RX] {"device_id":16' \
    "${TCP_SERVER_LOG}"; then

    echo "[ERROR] TCP server did not receive the JSON"
    exit 1
fi

echo "[PASS] TCP smoke test passed"
