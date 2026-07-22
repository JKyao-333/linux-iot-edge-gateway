#!/usr/bin/env bash

set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${HOME}/linux-iot-edge-gateway-build}"
GATEWAY_BIN="${BUILD_DIR}/edge_gateway"
ARTIFACT_DIR="${PROJECT_DIR}/artifacts"
SUMMARY_PATH="${ARTIFACT_DIR}/observability_test_summary.md"
RAW_LOG_PATH="${ARTIFACT_DIR}/observability_test_raw.log"
RUNTIME_DIR="$(mktemp -d /tmp/gateway-observability.XXXXXX)" || exit 1

SOCAT_PID=""
GATEWAY_PID=""
RESULT_LABELS=()
RESULT_STATUSES=()
RESULT_DETAILS=()
FAILED=0

cleanup() {
    if [[ -n "${GATEWAY_PID}" ]] && kill -0 "${GATEWAY_PID}" 2>/dev/null; then
        kill -TERM "${GATEWAY_PID}" 2>/dev/null || true
        wait "${GATEWAY_PID}" 2>/dev/null || true
    fi
    if [[ -n "${SOCAT_PID}" ]] && kill -0 "${SOCAT_PID}" 2>/dev/null; then
        kill "${SOCAT_PID}" 2>/dev/null || true
        wait "${SOCAT_PID}" 2>/dev/null || true
    fi
    rm -rf -- "${RUNTIME_DIR}"
}

trap cleanup EXIT INT TERM

record() {
    RESULT_LABELS+=("$1")
    RESULT_STATUSES+=("$2")
    RESULT_DETAILS+=("$3")
    if [[ "$2" == "FAIL" ]]; then
        FAILED=1
        echo "[FAIL] $1: $3" >&2
    else
        echo "[$2] $1: $3"
    fi
}

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[ERROR] required command not found: $1" >&2
        exit 1
    fi
}

http_request() {
    local url="$1"
    local body_path="$2"
    python3 - "${url}" "${body_path}" <<'PY'
from pathlib import Path
import sys
import urllib.error
import urllib.request

url = sys.argv[1]
body_path = Path(sys.argv[2])
try:
    with urllib.request.urlopen(url, timeout=2) as response:
        status = response.status
        body = response.read()
except urllib.error.HTTPError as error:
    status = error.code
    body = error.read()
except OSError:
    raise SystemExit(1)

body_path.write_bytes(body)
print(status)
PY
}

require_command cmake
require_command python3
require_command socat

mkdir -p -- "${ARTIFACT_DIR}" || {
    echo "[ERROR] cannot create artifacts directory" >&2
    exit 1
}

if [[ ! -x "${GATEWAY_BIN}" ]]; then
    echo "[INFO] configuring and building edge_gateway"
    cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
        || { echo "[ERROR] CMake configuration failed" >&2; exit 1; }
    cmake --build "${BUILD_DIR}" --target edge_gateway --parallel \
        || { echo "[ERROR] edge_gateway build failed" >&2; exit 1; }
fi

HTTP_PORT="$(python3 - <<'PY'
import socket
with socket.socket() as sock:
    sock.bind(("127.0.0.1", 0))
    print(sock.getsockname()[1])
PY
)" || exit 1

STM32_DEVICE="${RUNTIME_DIR}/tty_stm32"
GATEWAY_DEVICE="${RUNTIME_DIR}/tty_gateway"
CONFIG_PATH="${RUNTIME_DIR}/gateway.yaml"
GATEWAY_LOG="${RUNTIME_DIR}/gateway.log"

socat -d -d \
    "pty,raw,echo=0,link=${STM32_DEVICE}" \
    "pty,raw,echo=0,link=${GATEWAY_DEVICE}" \
    >"${RUNTIME_DIR}/socat.log" 2>&1 &
SOCAT_PID=$!

for _ in {1..30}; do
    [[ -e "${STM32_DEVICE}" && -e "${GATEWAY_DEVICE}" ]] && break
    sleep 0.1
done

if [[ ! -e "${GATEWAY_DEVICE}" ]]; then
    echo "[ERROR] socat did not create the virtual serial pair" >&2
    exit 1
fi

cat >"${CONFIG_PATH}" <<EOF
serial:
  devices:
    - ${GATEWAY_DEVICE}
  baud_rate: 115200
  reconnect_interval_seconds: 1
mqtt:
  host: localhost
  port: 1883
  topic_prefix: observability
  cache_retry_interval_seconds: 1
tcp:
  enabled: false
  host: 127.0.0.1
  port: 9000
http:
  enabled: true
  host: 127.0.0.1
  port: ${HTTP_PORT}
cache:
  type: sqlite
  path: ${RUNTIME_DIR}/pending_messages.db
log:
  path: ${RUNTIME_DIR}/gateway-file.log
  level: DEBUG
EOF

"${GATEWAY_BIN}" "${CONFIG_PATH}" >"${GATEWAY_LOG}" 2>&1 &
GATEWAY_PID=$!

BASE_URL="http://127.0.0.1:${HTTP_PORT}"
for _ in {1..50}; do
    if http_request "${BASE_URL}/health" "${RUNTIME_DIR}/health.json" \
        >"${RUNTIME_DIR}/health.code" 2>/dev/null; then
        break
    fi
    if ! kill -0 "${GATEWAY_PID}" 2>/dev/null; then
        break
    fi
    sleep 0.1
done

if ! kill -0 "${GATEWAY_PID}" 2>/dev/null; then
    record "Gateway startup" "FAIL" "process exited before HTTP checks"
else
    record "Gateway startup" "PASS" "process and HTTP listener are active"
fi

HEALTH_CODE="$(http_request "${BASE_URL}/health" "${RUNTIME_DIR}/health.json" 2>/dev/null || echo request-failed)"
if [[ "${HEALTH_CODE}" == "200" ]] \
    && grep -Fq '"status":"ok"' "${RUNTIME_DIR}/health.json" \
    && grep -Fq '"version":"' "${RUNTIME_DIR}/health.json" \
    && grep -Fq '"serial_workers":' "${RUNTIME_DIR}/health.json" \
    && grep -Fq '"cache_backend":"sqlite"' "${RUNTIME_DIR}/health.json"; then
    record "GET /health" "PASS" "HTTP 200 with runtime metadata"
else
    record "GET /health" "FAIL" "status=${HEALTH_CODE} or invalid JSON"
fi

READY_CODE="$(http_request "${BASE_URL}/ready" "${RUNTIME_DIR}/ready.json" 2>/dev/null || echo request-failed)"
if [[ "${READY_CODE}" == "200" || "${READY_CODE}" == "503" ]] \
    && grep -Eq '"ready":(true|false)' "${RUNTIME_DIR}/ready.json" \
    && grep -Fq '"checks":{' "${RUNTIME_DIR}/ready.json"; then
    record "GET /ready" "PASS" "HTTP ${READY_CODE} with readiness JSON"
else
    record "GET /ready" "FAIL" "status=${READY_CODE} or invalid JSON"
fi

METRICS_CODE="$(http_request "${BASE_URL}/metrics" "${RUNTIME_DIR}/metrics.txt" 2>/dev/null || echo request-failed)"
if [[ "${METRICS_CODE}" == "200" ]] \
    && grep -Fq 'iot_gateway_uptime_seconds' "${RUNTIME_DIR}/metrics.txt" \
    && grep -Fq 'iot_gateway_frames_parsed_total' "${RUNTIME_DIR}/metrics.txt" \
    && grep -Fq 'iot_gateway_cache_depth' "${RUNTIME_DIR}/metrics.txt"; then
    record "GET /metrics" "PASS" "Prometheus text contains required runtime metrics"
else
    record "GET /metrics" "FAIL" "status=${METRICS_CODE} or metrics missing"
fi

MISSING_CODE="$(http_request "${BASE_URL}/unknown" "${RUNTIME_DIR}/missing.json" 2>/dev/null || echo request-failed)"
if [[ "${MISSING_CODE}" == "404" ]]; then
    record "Unknown path" "PASS" "HTTP 404"
else
    record "Unknown path" "FAIL" "status=${MISSING_CODE}"
fi

kill -TERM "${GATEWAY_PID}" 2>/dev/null || true
wait "${GATEWAY_PID}"
GATEWAY_EXIT=$?
GATEWAY_PID=""

if (( GATEWAY_EXIT == 0 )) \
    && grep -Fq '[http] server stopped' "${GATEWAY_LOG}"; then
    record "SIGTERM shutdown" "PASS" "exit code 0 and HTTP server stopped"
else
    record "SIGTERM shutdown" "FAIL" "exit_code=${GATEWAY_EXIT} or stop log missing"
fi

{
    echo "# Observability Raw Log"
    echo
    echo "## Gateway"
    cat "${GATEWAY_LOG}"
    echo
    echo "## Health"
    cat "${RUNTIME_DIR}/health.json" 2>/dev/null || true
    echo
    echo "## Readiness"
    cat "${RUNTIME_DIR}/ready.json" 2>/dev/null || true
    echo
    echo "## Metrics"
    cat "${RUNTIME_DIR}/metrics.txt" 2>/dev/null || true
} >"${RAW_LOG_PATH}"

COLLECTED_AT="$(date --iso-8601=seconds 2>/dev/null || date)"
GIT_COMMIT="$(git -C "${PROJECT_DIR}" rev-parse HEAD 2>/dev/null || echo unknown)"
BUILD_DIR_LABEL="${BUILD_DIR/#${HOME}/<home>}"
OVERALL="PASS"
(( FAILED == 0 )) || OVERALL="FAIL"

{
    echo "# HTTP Observability Test"
    echo
    echo "- Collected at: ${COLLECTED_AT}"
    echo "- Git commit: \`${GIT_COMMIT}\`"
    echo "- Build directory: \`${BUILD_DIR_LABEL}\`"
    echo "- HTTP endpoint: \`127.0.0.1:<dynamic-port>\`"
    echo "- Overall: **${OVERALL}**"
    echo
    echo "| Check | Status | Detail |"
    echo "| --- | --- | --- |"
    for index in "${!RESULT_LABELS[@]}"; do
        echo "| ${RESULT_LABELS[$index]} | ${RESULT_STATUSES[$index]} | ${RESULT_DETAILS[$index]} |"
    done
    echo
    echo "This test validates local health, readiness, and metrics behavior. It does not establish production availability, fixed throughput, or latency guarantees."
} >"${SUMMARY_PATH}"

if (( FAILED != 0 )); then
    echo "[FAIL] observability test failed, report=artifacts/observability_test_summary.md" >&2
    exit 1
fi

echo "[PASS] observability test passed, report=artifacts/observability_test_summary.md"
