#!/usr/bin/env bash

set -eu

SOCAT_PID=""
SENDER_PID=""
GATEWAY_PID=""

cleanup() {
    if [[ -n "${GATEWAY_PID}" ]] \
        && kill -0 "${GATEWAY_PID}" 2>/dev/null; then

        kill -TERM "${GATEWAY_PID}" 2>/dev/null || true
        wait "${GATEWAY_PID}" 2>/dev/null || true
    fi
    if [[ -n "${SENDER_PID}" ]]; then
        kill "${SENDER_PID}" 2>/dev/null || true
        wait "${SENDER_PID}" 2>/dev/null || true
    fi
    if [[ -n "${SOCAT_PID}" ]]; then
        kill "${SOCAT_PID}" 2>/dev/null || true
        wait "${SOCAT_PID}" 2>/dev/null || true
    fi
}

trap cleanup EXIT INT TERM

rm -f /tmp/tty_stm32 /tmp/tty_gateway
socat \
    pty,raw,echo=0,link=/tmp/tty_stm32 \
    pty,raw,echo=0,link=/tmp/tty_gateway &
SOCAT_PID=$!

for _ in $(seq 1 30); do
    [[ -e /tmp/tty_stm32 && -e /tmp/tty_gateway ]] && break
    sleep 0.1
done

if [[ ! -e /tmp/tty_gateway ]]; then
    echo "[ERROR] virtual serial pair was not created" >&2
    exit 1
fi

python3 /app/scripts/mock_serial_sender.py /tmp/tty_stm32 &
SENDER_PID=$!

set +e
/app/edge_gateway /app/config/gateway.docker.yaml &
GATEWAY_PID=$!
wait "${GATEWAY_PID}"
EXIT_CODE=$?
GATEWAY_PID=""
set -e

exit "${EXIT_CODE}"
