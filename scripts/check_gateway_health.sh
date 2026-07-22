#!/usr/bin/env bash

set -u

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVICE_NAME="linux-iot-edge-gateway"
PROCESS_NAME="edge_gateway"
LOG_PATH=""
CACHE_DB=""
SERIAL_DEVICE=""
MQTT_HOST="localhost"
MQTT_PORT=1883
TCP_HOST=""
TCP_PORT=""
HTTP_URL=""
METRICS_URL=""
JSON_OUTPUT=false
OUTPUT_PATH=""
OUTPUT_EXPLICIT=false

CHECK_IDS=()
CHECK_LABELS=()
CHECK_STATUSES=()
CHECK_DETAILS=()
WARN_COUNT=0
FAIL_COUNT=0

usage() {
    cat <<'EOF'
Usage: check_gateway_health.sh [options]

Options:
  --service <name>       systemd service name
  --process-name <name>  gateway process name
  --log <path>           gateway log path
  --cache-db <path>      SQLite cache database
  --serial <device>      serial device
  --mqtt-host <host>     MQTT host, default: localhost
  --mqtt-port <port>     MQTT port, default: 1883
  --tcp-host <host>      optional TCP server host
  --tcp-port <port>      optional TCP server port
  --http-url <url>       optional HTTP health endpoint
  --metrics-url <url>    optional Prometheus metrics endpoint
  --json                 generate JSON instead of Markdown
  --output <path>        report output path
  -h, --help             show this help
EOF
}

argument_error() {
    echo "[ERROR] $1" >&2
    usage >&2
    exit 3
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --service)
            [[ $# -ge 2 ]] || argument_error "--service requires a value"
            SERVICE_NAME="$2"; shift 2 ;;
        --process-name)
            [[ $# -ge 2 ]] || argument_error "--process-name requires a value"
            PROCESS_NAME="$2"; shift 2 ;;
        --log)
            [[ $# -ge 2 ]] || argument_error "--log requires a path"
            LOG_PATH="$2"; shift 2 ;;
        --cache-db)
            [[ $# -ge 2 ]] || argument_error "--cache-db requires a path"
            CACHE_DB="$2"; shift 2 ;;
        --serial)
            [[ $# -ge 2 ]] || argument_error "--serial requires a device"
            SERIAL_DEVICE="$2"; shift 2 ;;
        --mqtt-host)
            [[ $# -ge 2 ]] || argument_error "--mqtt-host requires a value"
            MQTT_HOST="$2"; shift 2 ;;
        --mqtt-port)
            [[ $# -ge 2 ]] || argument_error "--mqtt-port requires a value"
            MQTT_PORT="$2"; shift 2 ;;
        --tcp-host)
            [[ $# -ge 2 ]] || argument_error "--tcp-host requires a value"
            TCP_HOST="$2"; shift 2 ;;
        --tcp-port)
            [[ $# -ge 2 ]] || argument_error "--tcp-port requires a value"
            TCP_PORT="$2"; shift 2 ;;
        --http-url)
            [[ $# -ge 2 ]] || argument_error "--http-url requires a value"
            HTTP_URL="$2"; shift 2 ;;
        --metrics-url)
            [[ $# -ge 2 ]] || argument_error "--metrics-url requires a value"
            METRICS_URL="$2"; shift 2 ;;
        --json)
            JSON_OUTPUT=true; shift ;;
        --output)
            [[ $# -ge 2 ]] || argument_error "--output requires a path"
            OUTPUT_PATH="$2"; OUTPUT_EXPLICIT=true; shift 2 ;;
        -h|--help)
            usage; exit 0 ;;
        *)
            argument_error "unknown option: $1" ;;
    esac
done

[[ -n "${SERVICE_NAME}" ]] || argument_error "service name must not be empty"
[[ -n "${PROCESS_NAME}" ]] || argument_error "process name must not be empty"
[[ "${MQTT_PORT}" =~ ^[1-9][0-9]*$ ]] \
    && (( MQTT_PORT <= 65535 )) \
    || argument_error "--mqtt-port must be between 1 and 65535"

if [[ -n "${TCP_HOST}" || -n "${TCP_PORT}" ]]; then
    [[ -n "${TCP_HOST}" && "${TCP_PORT}" =~ ^[1-9][0-9]*$ ]] \
        && (( TCP_PORT <= 65535 )) \
        || argument_error "--tcp-host and a valid --tcp-port must be provided together"
fi

if [[ "${OUTPUT_EXPLICIT}" == false ]]; then
    if [[ "${JSON_OUTPUT}" == true ]]; then
        OUTPUT_PATH="${PROJECT_DIR}/artifacts/health_check.json"
    else
        OUTPUT_PATH="${PROJECT_DIR}/artifacts/health_check.md"
    fi
fi

mkdir -p -- "$(dirname "${OUTPUT_PATH}")" 2>/dev/null \
    || { echo "[ERROR] cannot create output directory" >&2; exit 3; }

add_check() {
    local id="$1"
    local label="$2"
    local status="$3"
    local detail="$4"
    CHECK_IDS+=("${id}")
    CHECK_LABELS+=("${label}")
    CHECK_STATUSES+=("${status}")
    CHECK_DETAILS+=("${detail//$'\t'/ }")
    [[ "${status}" == "WARN" ]] && ((WARN_COUNT += 1))
    [[ "${status}" == "FAIL" ]] && ((FAIL_COUNT += 1))
}

safe_path_label() {
    local path="$1"

    if [[ "${path}" == /dev/serial/by-id/* ]]; then
        printf '/dev/serial/by-id/<redacted-serial>'
        return
    fi

    local normalized="${path/#${PROJECT_DIR}/<repo-root>}"
    normalized="${normalized/#${HOME}/<home>}"
    if [[ "${normalized}" == "${path}" ]]; then
        normalized="$(basename -- "${path}")"
    fi
    printf '%s' "${normalized}"
}

safe_endpoint_label() {
    local host="$1"
    local port="$2"
    case "${host}" in
        localhost|127.0.0.1|::1) printf 'localhost:%s' "${port}" ;;
        *) printf '<redacted-host>:%s' "${port}" ;;
    esac
}

check_endpoint() {
    local host="$1"
    local port="$2"
    if command -v python3 >/dev/null 2>&1; then
        python3 - "${host}" "${port}" <<'PY' >/dev/null 2>&1
import socket
import sys

try:
    with socket.create_connection((sys.argv[1], int(sys.argv[2])), timeout=2):
        pass
except OSError:
    raise SystemExit(1)
PY
        return $?
    fi
    if command -v nc >/dev/null 2>&1; then
        nc -z -w 2 "${host}" "${port}" >/dev/null 2>&1
        return $?
    fi
    return 127
}

safe_url_label() {
    local url="$1"
    python3 - "${url}" <<'PY' 2>/dev/null || printf '<redacted-url>'
import sys
from urllib.parse import urlsplit

parts = urlsplit(sys.argv[1])
host = parts.hostname or ""
if host not in {"localhost", "127.0.0.1", "::1"}:
    host = "<redacted-host>"
port = f":{parts.port}" if parts.port else ""
print(f"{parts.scheme or 'http'}://{host}{port}{parts.path or '/'}")
PY
}

fetch_url() {
    local url="$1"
    local body_path="$2"
    command -v python3 >/dev/null 2>&1 || return 127
    python3 - "${url}" "${body_path}" <<'PY'
from pathlib import Path
import sys
import urllib.error
import urllib.request

try:
    with urllib.request.urlopen(sys.argv[1], timeout=2) as response:
        status = response.status
        body = response.read()
except urllib.error.HTTPError as error:
    status = error.code
    body = error.read()
except OSError:
    raise SystemExit(1)

Path(sys.argv[2]).write_bytes(body)
print(status)
PY
}

COLLECTED_AT="$(date --iso-8601=seconds 2>/dev/null || date)"
GIT_COMMIT="unknown"
if git -C "${PROJECT_DIR}" rev-parse HEAD >/dev/null 2>&1; then
    GIT_COMMIT="$(git -C "${PROJECT_DIR}" rev-parse HEAD 2>/dev/null)"
    add_check git "Git commit" PASS "${GIT_COMMIT}"
else
    add_check git "Git commit" WARN "not a Git worktree"
fi

SYSTEMD_READY=false
if ! command -v systemctl >/dev/null 2>&1; then
    add_check systemd "systemd available" WARN "systemctl not found"
elif [[ ! -d /run/systemd/system ]]; then
    add_check systemd "systemd available" WARN "systemd is not the active init system"
else
    SYSTEMD_READY=true
    add_check systemd "systemd available" PASS "systemd is active"
fi

if [[ "${SYSTEMD_READY}" == true ]]; then
    if systemctl is-active --quiet "${SERVICE_NAME}" 2>/dev/null; then
        add_check service "Gateway service" PASS "${SERVICE_NAME} is active"
    else
        add_check service "Gateway service" FAIL "${SERVICE_NAME} is not active"
    fi
else
    add_check service "Gateway service" SKIP "systemd service state unavailable"
fi

if ! command -v pgrep >/dev/null 2>&1; then
    add_check process "Gateway process" WARN "pgrep not found"
elif pgrep -x "${PROCESS_NAME}" >/dev/null 2>&1; then
    process_count="$(pgrep -x "${PROCESS_NAME}" | wc -l | xargs)"
    add_check process "Gateway process" PASS "${process_count} process(es) found"
else
    add_check process "Gateway process" FAIL "${PROCESS_NAME} not found"
fi

if [[ -z "${LOG_PATH}" ]]; then
    add_check log "Recent log levels" SKIP "log path not provided"
elif [[ ! -r "${LOG_PATH}" ]]; then
    add_check log "Recent log levels" FAIL "$(safe_path_label "${LOG_PATH}") is not readable"
else
    error_count="$(tail -n 1000 -- "${LOG_PATH}" | grep -c '\[ERROR\]' || true)"
    warn_count="$(tail -n 1000 -- "${LOG_PATH}" | grep -c '\[WARN\]' || true)"
    if (( error_count > 0 || warn_count > 0 )); then
        add_check log "Recent log levels" WARN "ERROR=${error_count}, WARN=${warn_count} in last 1000 lines"
    else
        add_check log "Recent log levels" PASS "ERROR=0, WARN=0 in last 1000 lines"
    fi
fi

check_endpoint "${MQTT_HOST}" "${MQTT_PORT}"
endpoint_result=$?
if (( endpoint_result == 0 )); then
    add_check mqtt "MQTT endpoint" PASS "$(safe_endpoint_label "${MQTT_HOST}" "${MQTT_PORT}") is reachable"
elif (( endpoint_result == 127 )); then
    add_check mqtt "MQTT endpoint" WARN "python3 and nc are not available"
else
    add_check mqtt "MQTT endpoint" FAIL "$(safe_endpoint_label "${MQTT_HOST}" "${MQTT_PORT}") is unreachable"
fi

if [[ -z "${TCP_HOST}" ]]; then
    add_check tcp "TCP endpoint" SKIP "TCP endpoint not provided"
else
    check_endpoint "${TCP_HOST}" "${TCP_PORT}"
    endpoint_result=$?
    if (( endpoint_result == 0 )); then
        add_check tcp "TCP endpoint" PASS "$(safe_endpoint_label "${TCP_HOST}" "${TCP_PORT}") is reachable"
    elif (( endpoint_result == 127 )); then
        add_check tcp "TCP endpoint" WARN "python3 and nc are not available"
    else
        add_check tcp "TCP endpoint" FAIL "$(safe_endpoint_label "${TCP_HOST}" "${TCP_PORT}") is unreachable"
    fi
fi

if [[ -z "${CACHE_DB}" ]]; then
    add_check cache "SQLite queue depth" SKIP "cache database not provided"
elif [[ ! -f "${CACHE_DB}" ]]; then
    add_check cache "SQLite queue depth" FAIL "$(safe_path_label "${CACHE_DB}") does not exist"
elif ! command -v sqlite3 >/dev/null 2>&1; then
    add_check cache "SQLite queue depth" WARN "sqlite3 not found"
else
    cache_depth="$(sqlite3 "${CACHE_DB}" 'SELECT COUNT(*) FROM pending_messages;' 2>/dev/null)"
    if [[ "${cache_depth}" =~ ^[0-9]+$ ]]; then
        if (( cache_depth == 0 )); then
            add_check cache "SQLite queue depth" PASS "queue depth=${cache_depth}"
        else
            add_check cache "SQLite queue depth" WARN "queue depth=${cache_depth}"
        fi
    else
        add_check cache "SQLite queue depth" FAIL "cannot query pending_messages"
    fi
fi

if [[ -z "${SERIAL_DEVICE}" ]]; then
    add_check serial "Serial device" SKIP "serial device not provided"
elif [[ ! -e "${SERIAL_DEVICE}" ]]; then
    add_check serial "Serial device" FAIL "$(safe_path_label "${SERIAL_DEVICE}") does not exist"
elif [[ -r "${SERIAL_DEVICE}" && -w "${SERIAL_DEVICE}" ]]; then
    add_check serial "Serial device" PASS "$(safe_path_label "${SERIAL_DEVICE}") is readable and writable"
else
    add_check serial "Serial device" FAIL "$(safe_path_label "${SERIAL_DEVICE}") lacks read/write access"
fi

if [[ -z "${HTTP_URL}" ]]; then
    add_check http "HTTP health endpoint" SKIP "HTTP URL not provided"
else
    http_body="$(mktemp /tmp/gateway-health-http.XXXXXX)" \
        || { echo "[ERROR] cannot create temporary file" >&2; exit 3; }
    http_status="$(fetch_url "${HTTP_URL}" "${http_body}" 2>/dev/null)"
    http_result=$?
    if (( http_result == 127 )); then
        add_check http "HTTP health endpoint" WARN "python3 not found"
    elif (( http_result != 0 )); then
        add_check http "HTTP health endpoint" FAIL "$(safe_url_label "${HTTP_URL}") is unreachable"
    elif [[ "${http_status}" =~ ^2[0-9][0-9]$ ]]; then
        add_check http "HTTP health endpoint" PASS "$(safe_url_label "${HTTP_URL}") returned ${http_status}"
    else
        add_check http "HTTP health endpoint" FAIL "$(safe_url_label "${HTTP_URL}") returned ${http_status}"
    fi
    rm -f -- "${http_body}"
fi

if [[ -z "${METRICS_URL}" ]]; then
    add_check metrics "Prometheus metrics endpoint" SKIP "metrics URL not provided"
else
    metrics_body="$(mktemp /tmp/gateway-health-metrics.XXXXXX)" \
        || { echo "[ERROR] cannot create temporary file" >&2; exit 3; }
    metrics_status="$(fetch_url "${METRICS_URL}" "${metrics_body}" 2>/dev/null)"
    metrics_result=$?
    if (( metrics_result == 127 )); then
        add_check metrics "Prometheus metrics endpoint" WARN "python3 not found"
    elif (( metrics_result != 0 )); then
        add_check metrics "Prometheus metrics endpoint" FAIL "$(safe_url_label "${METRICS_URL}") is unreachable"
    elif [[ "${metrics_status}" =~ ^2[0-9][0-9]$ ]] \
        && grep -Fq 'iot_gateway_uptime_seconds' "${metrics_body}"; then
        add_check metrics "Prometheus metrics endpoint" PASS "$(safe_url_label "${METRICS_URL}") exposes gateway metrics"
    else
        add_check metrics "Prometheus metrics endpoint" FAIL "status=${metrics_status}; required metric missing"
    fi
    rm -f -- "${metrics_body}"
fi

if (( FAIL_COUNT > 0 )); then
    OVERALL="FAIL"
    EXIT_CODE=2
elif (( WARN_COUNT > 0 )); then
    OVERALL="WARN"
    EXIT_CODE=1
else
    OVERALL="PASS"
    EXIT_CODE=0
fi

TMP_CHECKS="$(mktemp /tmp/gateway-health.XXXXXX)" \
    || { echo "[ERROR] cannot create temporary file" >&2; exit 3; }
trap 'rm -f -- "${TMP_CHECKS}"' EXIT
for index in "${!CHECK_IDS[@]}"; do
    printf '%s\t%s\t%s\t%s\n' \
        "${CHECK_IDS[$index]}" \
        "${CHECK_LABELS[$index]}" \
        "${CHECK_STATUSES[$index]}" \
        "${CHECK_DETAILS[$index]}" >>"${TMP_CHECKS}"
done

if [[ "${JSON_OUTPUT}" == true ]]; then
    command -v python3 >/dev/null 2>&1 \
        || { echo "[ERROR] python3 is required for --json" >&2; exit 3; }
    if ! python3 - "${TMP_CHECKS}" "${OUTPUT_PATH}" "${COLLECTED_AT}" \
        "${GIT_COMMIT}" "${OVERALL}" <<'PY'
import json
from pathlib import Path
import sys

checks = []
for line in Path(sys.argv[1]).read_text(encoding="utf-8").splitlines():
    check_id, label, status, detail = line.split("\t", 3)
    checks.append({
        "id": check_id,
        "label": label,
        "status": status,
        "detail": detail,
    })

report = {
    "collected_at": sys.argv[3],
    "git_commit": sys.argv[4],
    "overall": sys.argv[5],
    "checks": checks,
}
Path(sys.argv[2]).write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + "\n",
    encoding="utf-8",
)
PY
    then
        echo "[ERROR] cannot write JSON report" >&2
        exit 3
    fi
else
    {
        echo "# Gateway Health Check"
        echo
        echo "- Collected at: ${COLLECTED_AT}"
        echo "- Git commit: \`${GIT_COMMIT}\`"
        echo "- Overall: **${OVERALL}**"
        echo
        echo "| Check | Status | Detail |"
        echo "| --- | --- | --- |"
        for index in "${!CHECK_IDS[@]}"; do
            echo "| ${CHECK_LABELS[$index]} | ${CHECK_STATUSES[$index]} | ${CHECK_DETAILS[$index]} |"
        done
        echo
        echo "This report describes current user-space gateway state only. It does not establish long-duration stability or field reliability."
    } >"${OUTPUT_PATH}" 2>/dev/null \
        || { echo "[ERROR] cannot write health report" >&2; exit 3; }
fi

echo "[${OVERALL}] health check completed, report=$(safe_path_label "${OUTPUT_PATH}")"
exit "${EXIT_CODE}"
