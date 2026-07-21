#!/usr/bin/env bash

set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DURATION_SECONDS=300
RATE_HZ=1
SERIAL_DEVICE=""
CONFIG_SOURCE="${PROJECT_DIR}/config/gateway.yaml"
BUILD_DIR="${HOME}/linux-iot-edge-gateway-build"
MQTT_TOPIC="sensor/+/data"
WITH_TCP=false
OUTPUT_DIR=""
COLLECT_ENV=false
NO_BUILD=false
HARDWARE_LABEL=""
SAMPLE_INTERVAL_SECONDS="${SAMPLE_INTERVAL_SECONDS:-5}"

usage() {
    cat <<'EOF'
Usage: run_stability_test.sh [options]

Options:
  --duration-seconds <N>   Test duration, default: 300
  --rate-hz <N>            Virtual sender rate, default: 1
  --serial <device>        Use a real serial device instead of socat
  --config <path>          Gateway YAML, default: config/gateway.yaml
  --build-dir <path>       Build directory
  --mqtt-topic <topic>     Subscription topic, default: sensor/+/data
  --with-tcp               Start and verify the Python TCP server
  --output-dir <path>      Output directory
  --collect-env            Generate env_report.md in the output directory
  --no-build               Skip CMake configure and build
  --hardware-label <text>  Hardware/source label for the summary
  -h, --help               Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --duration-seconds)
            [[ $# -ge 2 ]] || { echo "[ERROR] --duration-seconds requires a value" >&2; exit 2; }
            DURATION_SECONDS="$2"
            shift 2
            ;;
        --rate-hz)
            [[ $# -ge 2 ]] || { echo "[ERROR] --rate-hz requires a value" >&2; exit 2; }
            RATE_HZ="$2"
            shift 2
            ;;
        --serial)
            [[ $# -ge 2 ]] || { echo "[ERROR] --serial requires a device" >&2; exit 2; }
            SERIAL_DEVICE="$2"
            shift 2
            ;;
        --config)
            [[ $# -ge 2 ]] || { echo "[ERROR] --config requires a path" >&2; exit 2; }
            CONFIG_SOURCE="$2"
            shift 2
            ;;
        --build-dir)
            [[ $# -ge 2 ]] || { echo "[ERROR] --build-dir requires a path" >&2; exit 2; }
            BUILD_DIR="$2"
            shift 2
            ;;
        --mqtt-topic)
            [[ $# -ge 2 ]] || { echo "[ERROR] --mqtt-topic requires a topic" >&2; exit 2; }
            MQTT_TOPIC="$2"
            shift 2
            ;;
        --with-tcp)
            WITH_TCP=true
            shift
            ;;
        --output-dir)
            [[ $# -ge 2 ]] || { echo "[ERROR] --output-dir requires a path" >&2; exit 2; }
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --collect-env)
            COLLECT_ENV=true
            shift
            ;;
        --no-build)
            NO_BUILD=true
            shift
            ;;
        --hardware-label)
            [[ $# -ge 2 ]] || { echo "[ERROR] --hardware-label requires a value" >&2; exit 2; }
            HARDWARE_LABEL="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "[ERROR] unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ ! "${DURATION_SECONDS}" =~ ^[1-9][0-9]*$ ]]; then
    echo "[ERROR] --duration-seconds must be a positive integer" >&2
    exit 2
fi

if ! python3 - "${RATE_HZ}" <<'PY' >/dev/null 2>&1
import sys
value = float(sys.argv[1])
raise SystemExit(0 if value > 0 else 1)
PY
then
    echo "[ERROR] --rate-hz must be greater than zero" >&2
    exit 2
fi

if [[ ! "${SAMPLE_INTERVAL_SECONDS}" =~ ^[1-9][0-9]*$ ]]; then
    echo "[ERROR] SAMPLE_INTERVAL_SECONDS must be a positive integer" >&2
    exit 2
fi

timestamp="$(date +%Y%m%d_%H%M%S)"
OUTPUT_DIR="${OUTPUT_DIR:-${PROJECT_DIR}/artifacts/stability_${timestamp}}"
mkdir -p -- "${OUTPUT_DIR}" || {
    echo "[ERROR] cannot create output directory: ${OUTPUT_DIR}" >&2
    exit 1
}

SUMMARY_PATH="${OUTPUT_DIR}/stability_summary.md"
SAMPLES_PATH="${OUTPUT_DIR}/stability_samples.csv"
GATEWAY_LOG="${OUTPUT_DIR}/gateway.log"
GATEWAY_CONSOLE_LOG="${OUTPUT_DIR}/gateway_console.log"
MQTT_RECEIVED_LOG="${OUTPUT_DIR}/mqtt_received.log"
TCP_RECEIVED_LOG="${OUTPUT_DIR}/tcp_received.log"
SIMULATOR_LOG="${OUTPUT_DIR}/simulator.log"
RUNTIME_CONFIG="${OUTPUT_DIR}/gateway.runtime.yaml"
RUNTIME_DIR="$(mktemp -d /tmp/iot-gateway-stability.XXXXXX)"
STM32_DEVICE="${RUNTIME_DIR}/tty_stm32"
VIRTUAL_GATEWAY_DEVICE="${RUNTIME_DIR}/tty_gateway"

SOCAT_PID=""
BROKER_PID=""
TCP_PID=""
SUBSCRIBER_PID=""
SIMULATOR_PID=""
GATEWAY_PID=""
GATEWAY_EXIT_CODE=""

terminate_process() {
    local pid="${1:-}"
    if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
        kill -TERM "${pid}" 2>/dev/null || true
        for _ in {1..30}; do
            kill -0 "${pid}" 2>/dev/null || break
            sleep 0.1
        done
        kill -KILL "${pid}" 2>/dev/null || true
        wait "${pid}" 2>/dev/null || true
    fi
}

cleanup() {
    terminate_process "${GATEWAY_PID}"
    terminate_process "${SIMULATOR_PID}"
    terminate_process "${SUBSCRIBER_PID}"
    terminate_process "${TCP_PID}"
    terminate_process "${BROKER_PID}"
    terminate_process "${SOCAT_PID}"
    rm -rf -- "${RUNTIME_DIR}"
}

trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[ERROR] required command not found: $1" >&2
        return 1
    fi
}

yaml_value() {
    local section="$1"
    local key="$2"
    local fallback="$3"
    local value

    value="$(awk -v wanted_section="${section}" -v wanted_key="${key}" '
        /^[^[:space:]#][^:]*:/ {
            current = $1
            sub(/:$/, "", current)
        }
        current == wanted_section && $1 == wanted_key ":" {
            $1 = ""
            sub(/^[[:space:]]+/, "")
            gsub(/^\"|\"$/, "")
            print
            exit
        }
    ' "${CONFIG_SOURCE}" 2>/dev/null)"

    printf '%s\n' "${value:-${fallback}}"
}

find_free_port() {
    python3 - <<'PY'
import socket

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
    sock.bind(("127.0.0.1", 0))
    print(sock.getsockname()[1])
PY
}

port_is_listening() {
    local host="$1"
    local port="$2"
    python3 - "${host}" "${port}" <<'PY' >/dev/null 2>&1
import socket
import sys

try:
    with socket.create_connection((sys.argv[1], int(sys.argv[2])), timeout=1):
        pass
except OSError:
    raise SystemExit(1)
PY
}

wait_for_file_pattern() {
    local file="$1"
    local pattern="$2"
    local timeout_seconds="$3"
    local elapsed=0

    while (( elapsed < timeout_seconds * 10 )); do
        if grep -Fq -- "${pattern}" "${file}" 2>/dev/null; then
            return 0
        fi
        sleep 0.1
        ((elapsed += 1))
    done
    return 1
}

count_lines() {
    local file="$1"
    [[ -f "${file}" ]] || { echo 0; return; }
    grep -cve '^[[:space:]]*$' "${file}" 2>/dev/null || true
}

count_pattern() {
    local file="$1"
    local pattern="$2"
    [[ -f "${file}" ]] || { echo 0; return; }
    grep -Ec -- "${pattern}" "${file}" 2>/dev/null || true
}

cache_depth() {
    local cache_path="$1"
    if [[ ! -f "${cache_path}" ]]; then
        echo 0
        return
    fi
    sqlite3 "${cache_path}" \
        'SELECT COUNT(*) FROM pending_messages;' \
        2>/dev/null || echo NA
}

preflight_ok=true
for dependency in python3 mosquitto_pub mosquitto_sub sqlite3; do
    require_command "${dependency}" || preflight_ok=false
done

if [[ "${NO_BUILD}" != true ]]; then
    require_command cmake || preflight_ok=false
fi

if [[ -z "${SERIAL_DEVICE}" ]]; then
    require_command socat || preflight_ok=false
    if ! python3 -c 'import serial' >/dev/null 2>&1; then
        echo "[ERROR] Python module 'serial' not found; install python3-serial or pyserial" >&2
        preflight_ok=false
    fi
fi

if [[ "${preflight_ok}" != true ]]; then
    echo "[FAIL] stability test prerequisites are incomplete" >&2
    exit 1
fi

CONFIG_SOURCE="$(realpath "${CONFIG_SOURCE}" 2>/dev/null || printf '%s' "${CONFIG_SOURCE}")"
if [[ ! -r "${CONFIG_SOURCE}" ]]; then
    echo "[ERROR] configuration file is not readable: ${CONFIG_SOURCE}" >&2
    exit 1
fi

if [[ "${NO_BUILD}" != true ]]; then
    echo "[INFO] configuring project"
    cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" || exit 1
    echo "[INFO] building edge_gateway"
    cmake --build "${BUILD_DIR}" --target edge_gateway --parallel || exit 1
fi

GATEWAY_BIN="${BUILD_DIR}/edge_gateway"
if [[ ! -x "${GATEWAY_BIN}" ]]; then
    echo "[ERROR] gateway executable not found: ${GATEWAY_BIN}" >&2
    echo "[INFO] remove --no-build or provide the correct --build-dir" >&2
    exit 1
fi

MQTT_HOST="$(yaml_value mqtt host localhost)"
MQTT_PORT="$(yaml_value mqtt port 1883)"
TCP_HOST="$(yaml_value tcp host 127.0.0.1)"
TCP_PORT="$(yaml_value tcp port 9000)"
CACHE_PATH="${OUTPUT_DIR}/pending_messages.db"

if [[ -z "${SERIAL_DEVICE}" ]]; then
    SERIAL_SOURCE="socat virtual PTY pair"
    SERIAL_DEVICE="${VIRTUAL_GATEWAY_DEVICE}"
    HARDWARE_LABEL="${HARDWARE_LABEL:-virtual-socat}"
else
    SERIAL_SOURCE="real serial device"
    HARDWARE_LABEL="${HARDWARE_LABEL:-unspecified-hardware}"
    if [[ ! -e "${SERIAL_DEVICE}" || ! -r "${SERIAL_DEVICE}" ]]; then
        echo "[ERROR] serial device is missing or unreadable: ${SERIAL_DEVICE}" >&2
        exit 1
    fi
fi

if ! python3 - "${CONFIG_SOURCE}" "${RUNTIME_CONFIG}" \
    "${CACHE_PATH}" "${GATEWAY_LOG}" "${WITH_TCP}" <<'PY'
import json
import pathlib
import sys

source, destination, cache_path, log_path, with_tcp = sys.argv[1:]
section = None
output = []

for line in pathlib.Path(source).read_text(encoding="utf-8").splitlines():
    stripped = line.strip()
    if line and not line[0].isspace() and stripped.endswith(":"):
        section = stripped[:-1]
    key = stripped.split(":", 1)[0] if ":" in stripped else ""
    if section == "tcp" and key == "enabled":
        output.append(f"  enabled: {with_tcp.lower()}")
    elif section == "cache" and key == "path":
        output.append(f"  path: {json.dumps(cache_path)}")
    elif section == "log" and key == "path":
        output.append(f"  path: {json.dumps(log_path)}")
    else:
        output.append(line)

pathlib.Path(destination).write_text("\n".join(output) + "\n", encoding="utf-8")
PY
then
    echo "[ERROR] failed to create the isolated runtime configuration" >&2
    exit 1
fi

if [[ "${COLLECT_ENV}" == true ]]; then
    echo "[INFO] collecting environment"
    "${PROJECT_DIR}/scripts/collect_env.sh" \
        --include-git \
        --include-packages \
        --include-hash "${GATEWAY_BIN}" \
        --output "${OUTPUT_DIR}/env_report.md" || {
        echo "[WARN] environment report could not be generated"
    }
fi

if ! mosquitto_pub -h "${MQTT_HOST}" -p "${MQTT_PORT}" \
    -t stability/probe -n >/dev/null 2>&1; then
    if [[ "${MQTT_HOST}" == "localhost" || "${MQTT_HOST}" == "127.0.0.1" ]]; then
        require_command mosquitto || {
            echo "[ERROR] Mosquitto is not running and the broker command is unavailable" >&2
            exit 1
        }
        echo "[INFO] starting local Mosquitto broker on ${MQTT_PORT}"
        mosquitto -p "${MQTT_PORT}" >>"${OUTPUT_DIR}/mosquitto.log" 2>&1 &
        BROKER_PID=$!
        for _ in {1..50}; do
            mosquitto_pub -h "${MQTT_HOST}" -p "${MQTT_PORT}" \
                -t stability/probe -n >/dev/null 2>&1 && break
            sleep 0.1
        done
    else
        echo "[ERROR] configured MQTT Broker is unavailable: ${MQTT_HOST}:${MQTT_PORT}" >&2
        exit 1
    fi
fi

if ! mosquitto_pub -h "${MQTT_HOST}" -p "${MQTT_PORT}" \
    -t stability/probe -n >/dev/null 2>&1; then
    echo "[ERROR] MQTT Broker did not become ready: ${MQTT_HOST}:${MQTT_PORT}" >&2
    exit 1
fi

: >"${MQTT_RECEIVED_LOG}"
mosquitto_sub -h "${MQTT_HOST}" -p "${MQTT_PORT}" \
    -t "${MQTT_TOPIC}" -v >>"${MQTT_RECEIVED_LOG}" 2>&1 &
SUBSCRIBER_PID=$!
sleep 0.3
if ! kill -0 "${SUBSCRIBER_PID}" 2>/dev/null; then
    echo "[ERROR] MQTT subscriber failed to start" >&2
    exit 1
fi

if [[ "${WITH_TCP}" == true ]]; then
    if port_is_listening "${TCP_HOST}" "${TCP_PORT}"; then
        echo "[ERROR] TCP endpoint ${TCP_HOST}:${TCP_PORT} is already in use" >&2
        echo "[INFO] stop the existing server so this run can collect an isolated TCP log" >&2
        exit 1
    fi
    if [[ "${TCP_HOST}" != "localhost" && "${TCP_HOST}" != "127.0.0.1" ]]; then
        echo "[ERROR] --with-tcp currently requires a local TCP host in the configuration" >&2
        exit 1
    fi
    : >"${TCP_RECEIVED_LOG}"
    python3 -u "${PROJECT_DIR}/scripts/mock_tcp_server.py" \
        "${TCP_PORT}" >>"${TCP_RECEIVED_LOG}" 2>&1 &
    TCP_PID=$!
    if ! wait_for_file_pattern "${TCP_RECEIVED_LOG}" "TCP server listening" 5; then
        echo "[ERROR] Python TCP server failed to start on port ${TCP_PORT}" >&2
        exit 1
    fi
fi

if [[ "${SERIAL_SOURCE}" == "socat virtual PTY pair" ]]; then
    rm -f -- "${STM32_DEVICE}" "${SERIAL_DEVICE}"
    socat -d -d \
        "pty,raw,echo=0,link=${STM32_DEVICE}" \
        "pty,raw,echo=0,link=${SERIAL_DEVICE}" \
        >>"${OUTPUT_DIR}/socat.log" 2>&1 &
    SOCAT_PID=$!
    for _ in {1..50}; do
        [[ -e "${STM32_DEVICE}" && -e "${SERIAL_DEVICE}" ]] && break
        sleep 0.1
    done
    if [[ ! -e "${STM32_DEVICE}" || ! -e "${SERIAL_DEVICE}" ]]; then
        echo "[ERROR] socat did not create the virtual serial pair" >&2
        exit 1
    fi
fi

: >"${GATEWAY_CONSOLE_LOG}"
echo "[INFO] starting edge_gateway"
"${GATEWAY_BIN}" "${RUNTIME_CONFIG}" "${SERIAL_DEVICE}" \
    >>"${GATEWAY_CONSOLE_LOG}" 2>&1 &
GATEWAY_PID=$!

if ! wait_for_file_pattern "${GATEWAY_LOG}" "serial opened" 10; then
    echo "[ERROR] edge_gateway did not open the serial source" >&2
    exit 1
fi

if [[ "${SERIAL_SOURCE}" == "socat virtual PTY pair" ]]; then
    echo "[INFO] starting virtual sensor sender at ${RATE_HZ} Hz"
    python3 -u "${PROJECT_DIR}/scripts/mock_serial_sender.py" \
        "${STM32_DEVICE}" --rate-hz "${RATE_HZ}" \
        >>"${SIMULATOR_LOG}" 2>&1 &
    SIMULATOR_PID=$!
fi

printf '%s\n' \
    'timestamp,alive,cpu_percent,rss_kb,fd_count,thread_count,error_count,warn_count,cache_depth,mqtt_count,tcp_count,simulator_sent' \
    >"${SAMPLES_PATH}"

TEST_START_ISO="$(date --iso-8601=seconds 2>/dev/null || date)"
TEST_START_EPOCH="$(date +%s)"
TEST_END_EPOCH="$((TEST_START_EPOCH + DURATION_SECONDS))"
PEAK_CACHE=0
METRIC_INCOMPLETE=false
PROCESS_DIED=false

echo "[INFO] sampling for ${DURATION_SECONDS} seconds"
while (( $(date +%s) < TEST_END_EPOCH )); do
    now_iso="$(date --iso-8601=seconds 2>/dev/null || date)"
    if kill -0 "${GATEWAY_PID}" 2>/dev/null; then
        alive=1
        cpu_percent="$(ps -p "${GATEWAY_PID}" -o %cpu= 2>/dev/null | xargs || true)"
        rss_kb="$(ps -p "${GATEWAY_PID}" -o rss= 2>/dev/null | xargs || true)"
        fd_count="$(find "/proc/${GATEWAY_PID}/fd" -mindepth 1 -maxdepth 1 2>/dev/null | wc -l | xargs)"
        thread_count="$(find "/proc/${GATEWAY_PID}/task" -mindepth 1 -maxdepth 1 2>/dev/null | wc -l | xargs)"
    else
        alive=0
        cpu_percent=NA
        rss_kb=NA
        fd_count=NA
        thread_count=NA
        PROCESS_DIED=true
    fi

    for metric_name in cpu_percent rss_kb fd_count thread_count; do
        if [[ -z "${!metric_name}" ]]; then
            printf -v "${metric_name}" '%s' NA
            METRIC_INCOMPLETE=true
        fi
    done

    error_count="$(count_pattern "${GATEWAY_LOG}" '\[ERROR\]')"
    warn_count="$(count_pattern "${GATEWAY_LOG}" '\[WARN\]')"
    current_cache="$(cache_depth "${CACHE_PATH}")"
    if [[ "${current_cache}" =~ ^[0-9]+$ ]]; then
        (( current_cache > PEAK_CACHE )) && PEAK_CACHE="${current_cache}"
    else
        METRIC_INCOMPLETE=true
    fi
    mqtt_count="$(count_lines "${MQTT_RECEIVED_LOG}")"
    if [[ "${WITH_TCP}" == true ]]; then
        tcp_count="$(count_pattern "${TCP_RECEIVED_LOG}" '^\[RX\] ')"
    else
        tcp_count=NA
    fi
    if [[ "${SERIAL_SOURCE}" == "socat virtual PTY pair" ]]; then
        simulator_sent="$(count_pattern "${SIMULATOR_LOG}" '^TX frame #')"
    else
        simulator_sent=NA
    fi

    printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
        "${now_iso}" "${alive}" "${cpu_percent}" "${rss_kb}" \
        "${fd_count}" "${thread_count}" "${error_count}" "${warn_count}" \
        "${current_cache}" "${mqtt_count}" "${tcp_count}" "${simulator_sent}" \
        >>"${SAMPLES_PATH}"

    [[ "${PROCESS_DIED}" == false ]] || break
    remaining="$((TEST_END_EPOCH - $(date +%s)))"
    (( remaining > 0 )) || break
    sleep_for="${SAMPLE_INTERVAL_SECONDS}"
    (( remaining < sleep_for )) && sleep_for="${remaining}"
    sleep "${sleep_for}"
done

if kill -0 "${GATEWAY_PID}" 2>/dev/null; then
    kill -TERM "${GATEWAY_PID}" 2>/dev/null || true
    wait "${GATEWAY_PID}" 2>/dev/null
    GATEWAY_EXIT_CODE=$?
else
    wait "${GATEWAY_PID}" 2>/dev/null
    GATEWAY_EXIT_CODE=$?
fi
GATEWAY_PID=""

terminate_process "${SIMULATOR_PID}"
SIMULATOR_PID=""
sleep 0.5

TEST_END_ISO="$(date --iso-8601=seconds 2>/dev/null || date)"
MQTT_COUNT="$(count_lines "${MQTT_RECEIVED_LOG}")"
TCP_COUNT=NA
[[ "${WITH_TCP}" == true ]] && TCP_COUNT="$(count_pattern "${TCP_RECEIVED_LOG}" '^\[RX\] ')"
SIMULATOR_COUNT=NA
[[ "${SERIAL_SOURCE}" == "socat virtual PTY pair" ]] \
    && SIMULATOR_COUNT="$(count_pattern "${SIMULATOR_LOG}" '^TX frame #')"
ERROR_COUNT="$(count_pattern "${GATEWAY_LOG}" '\[ERROR\]')"
WARN_COUNT="$(count_pattern "${GATEWAY_LOG}" '\[WARN\]')"
FINAL_CACHE="$(cache_depth "${CACHE_PATH}")"
CRASH_COUNT="$(count_pattern "${GATEWAY_CONSOLE_LOG}" 'Segmentation fault|Aborted|terminate called|core dumped')"

CONCLUSION=PASS
REASONS=()

if [[ "${PROCESS_DIED}" == true || "${GATEWAY_EXIT_CODE}" -ne 0 || "${CRASH_COUNT}" -gt 0 ]]; then
    CONCLUSION=FAIL
    REASONS+=("edge_gateway exited abnormally or emitted a crash marker")
fi
if [[ ! "${MQTT_COUNT}" =~ ^[0-9]+$ || "${MQTT_COUNT}" -lt 1 ]]; then
    CONCLUSION=FAIL
    REASONS+=("no MQTT sensor message was observed")
fi
if [[ "${WITH_TCP}" == true ]] && \
    { [[ ! "${TCP_COUNT}" =~ ^[0-9]+$ ]] || [[ "${TCP_COUNT}" -lt 1 ]]; }; then
    CONCLUSION=FAIL
    REASONS+=("no TCP JSON Lines message was observed")
fi
if [[ "${FINAL_CACHE}" =~ ^[0-9]+$ ]]; then
    if (( FINAL_CACHE > 0 )); then
        CONCLUSION=FAIL
        REASONS+=("SQLite cache remained non-empty without Broker fault injection")
    fi
else
    METRIC_INCOMPLETE=true
    REASONS+=("SQLite cache depth could not be collected")
fi
if [[ "${CONCLUSION}" == PASS && "${METRIC_INCOMPLETE}" == true ]]; then
    CONCLUSION=INCONCLUSIVE
    REASONS+=("one or more process metrics were unavailable")
fi
if (( ${#REASONS[@]} == 0 )); then
    REASONS+=("all configured continuity checks completed")
fi

git_commit="$(git -C "${PROJECT_DIR}" rev-parse HEAD 2>/dev/null || echo unknown)"
display_build="${BUILD_DIR/#${HOME}/<home>}"
display_config="${CONFIG_SOURCE/#${PROJECT_DIR}/<repo-root>}"
display_output="${OUTPUT_DIR/#${PROJECT_DIR}/<repo-root>}"
display_serial="${SERIAL_DEVICE}"
[[ "${SERIAL_SOURCE}" == "real serial device" ]] || display_serial="virtual PTY"

{
    echo "# Stability Test Summary"
    echo
    echo "## Test Configuration"
    echo
    echo "- Conclusion: **${CONCLUSION}**"
    echo "- Git commit: \`${git_commit}\`"
    echo "- Hardware label: \`${HARDWARE_LABEL}\`"
    echo "- Start: ${TEST_START_ISO}"
    echo "- End: ${TEST_END_ISO}"
    echo "- Requested duration: ${DURATION_SECONDS} seconds"
    echo "- Configured source rate: ${RATE_HZ} Hz"
    echo "- Serial source: ${SERIAL_SOURCE} (${display_serial})"
    echo "- Build directory: \`${display_build}\`"
    echo "- Configuration: \`${display_config}\`"
    echo "- Output directory: \`${display_output}\`"
    echo "- MQTT: ${MQTT_HOST}:${MQTT_PORT}, subscription \`${MQTT_TOPIC}\`"
    if [[ "${WITH_TCP}" == true ]]; then
        echo "- TCP: enabled, ${TCP_HOST}:${TCP_PORT}"
    else
        echo "- TCP: disabled for this run"
    fi
    echo
    echo "## Observations"
    echo
    echo "- Simulator messages sent: ${SIMULATOR_COUNT}"
    echo "- MQTT messages received: ${MQTT_COUNT}"
    echo "- TCP messages received: ${TCP_COUNT}"
    echo "- Peak SQLite cache depth: ${PEAK_CACHE}"
    echo "- Final SQLite cache depth: ${FINAL_CACHE}"
    echo "- Gateway WARN entries: ${WARN_COUNT}"
    echo "- Gateway ERROR entries: ${ERROR_COUNT}"
    echo "- Gateway exited unexpectedly during the interval: ${PROCESS_DIED}"
    echo "- Gateway termination exit code: ${GATEWAY_EXIT_CODE}"
    echo
    echo "## Decision Notes"
    echo
    for reason in "${REASONS[@]}"; do
        echo "- ${reason}"
    done
    echo
    echo "## Engineering Boundary"
    echo
    echo "This workflow observes continuous execution and abnormal trends in the selected environment. It does not establish fixed throughput, P99 latency, continuous production availability, industrial field reliability, or mass-production readiness. Formal performance evaluation requires a separate workload, timing, and resource measurement design."
} >"${SUMMARY_PATH}"

echo "[${CONCLUSION}] stability test completed"
echo "[INFO] summary=${SUMMARY_PATH}"
echo "[INFO] samples=${SAMPLES_PATH}"

case "${CONCLUSION}" in
    PASS) exit 0 ;;
    FAIL) exit 1 ;;
    INCONCLUSIVE) exit 2 ;;
esac
