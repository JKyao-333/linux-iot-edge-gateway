#!/usr/bin/env bash

set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${HOME}/linux-iot-edge-gateway-build}"
OUTPUT_DIR="${OUTPUT_DIR:-${PROJECT_DIR}/artifacts}"
SUMMARY_PATH="${OUTPUT_DIR}/fault_injection_summary.md"
RAW_LOG="${OUTPUT_DIR}/fault_injection_raw.log"
RUNTIME_DIR="$(mktemp -d /tmp/iot-gateway-fault.XXXXXX)"

STM32_DEVICE="${RUNTIME_DIR}/tty_stm32"
GATEWAY_DEVICE="${RUNTIME_DIR}/tty_gateway"
CONFIG_PATH="${RUNTIME_DIR}/gateway.yaml"
CACHE_PATH="${RUNTIME_DIR}/pending_messages.db"
GATEWAY_FILE_LOG="${RUNTIME_DIR}/gateway.log"
MQTT_RECEIVED_LOG="${RUNTIME_DIR}/mqtt_received.log"
TCP_RECEIVED_LOG="${RUNTIME_DIR}/tcp_received.log"

SOCAT_PID=""
BROKER_PID=""
TCP_PID=""
SUBSCRIBER_PID=""
GATEWAY_PID=""

TEST_START="$(date --iso-8601=seconds 2>/dev/null || date)"
SCENARIO_IDS=()
SCENARIO_NAMES=()
SCENARIO_RESULTS=()
SCENARIO_DETAILS=()

mkdir -p -- "${OUTPUT_DIR}"
: >"${RAW_LOG}"
: >"${MQTT_RECEIVED_LOG}"
: >"${TCP_RECEIVED_LOG}"

log() {
    printf '%s\n' "$*" | tee -a "${RAW_LOG}"
}

terminate_process() {
    local pid="${1:-}"
    if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
        kill "${pid}" 2>/dev/null || true
        wait "${pid}" 2>/dev/null || true
    fi
}

cleanup() {
    terminate_process "${GATEWAY_PID}"
    terminate_process "${SUBSCRIBER_PID}"
    terminate_process "${TCP_PID}"
    terminate_process "${BROKER_PID}"
    terminate_process "${SOCAT_PID}"
    rm -rf -- "${RUNTIME_DIR}"
}

trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

record_result() {
    SCENARIO_IDS+=("$1")
    SCENARIO_NAMES+=("$2")
    SCENARIO_RESULTS+=("$3")
    SCENARIO_DETAILS+=("$4")
    printf '[%s] %s %s: %s\n' "$3" "$1" "$2" "$4" | tee -a "${RAW_LOG}"
}

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        log "[ERROR] required command not found: $1"
        return 1
    fi
    return 0
}

wait_for_log_count() {
    local file="$1"
    local pattern="$2"
    local expected="$3"
    local timeout_seconds="$4"
    local elapsed=0
    local count=0

    while (( elapsed < timeout_seconds * 10 )); do
        count="$(grep -Fc -- "${pattern}" "${file}" 2>/dev/null || true)"
        if (( count >= expected )); then
            return 0
        fi
        sleep 0.1
        ((elapsed += 1))
    done
    return 1
}

line_count() {
    local file="$1"
    [[ -f "${file}" ]] || { echo 0; return; }
    grep -cve '^[[:space:]]*$' "${file}" 2>/dev/null || true
}

pattern_count() {
    grep -Fc -- "$2" "$1" 2>/dev/null || true
}

cache_count() {
    if [[ ! -f "${CACHE_PATH}" ]]; then
        echo 0
        return
    fi
    sqlite3 "${CACHE_PATH}" \
        'SELECT COUNT(*) FROM pending_messages;' \
        2>/dev/null || echo -1
}

wait_for_cache_at_least() {
    local expected="$1"
    local timeout_seconds="$2"
    local elapsed=0
    local count=0

    while (( elapsed < timeout_seconds * 10 )); do
        count="$(cache_count)"
        if [[ "${count}" =~ ^[0-9]+$ ]] && (( count >= expected )); then
            return 0
        fi
        sleep 0.1
        ((elapsed += 1))
    done
    return 1
}

wait_for_cache_zero() {
    local timeout_seconds="$1"
    local elapsed=0
    local count=0

    while (( elapsed < timeout_seconds * 10 )); do
        count="$(cache_count)"
        if [[ "${count}" == "0" ]]; then
            return 0
        fi
        sleep 0.1
        ((elapsed += 1))
    done
    return 1
}

find_free_port() {
    python3 - <<'PY'
import socket

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
    sock.bind(("127.0.0.1", 0))
    print(sock.getsockname()[1])
PY
}

start_broker() {
    mosquitto -p "${MQTT_PORT}" >>"${RAW_LOG}" 2>&1 &
    BROKER_PID=$!

    for _ in {1..40}; do
        if mosquitto_pub -h 127.0.0.1 -p "${MQTT_PORT}" \
            -t fi/probe -n >/dev/null 2>&1; then
            return 0
        fi
        sleep 0.1
    done
    return 1
}

start_subscriber() {
    mosquitto_sub -h 127.0.0.1 -p "${MQTT_PORT}" \
        -t 'fi/+/data' -v >>"${MQTT_RECEIVED_LOG}" 2>&1 &
    SUBSCRIBER_PID=$!
    sleep 0.3
    kill -0 "${SUBSCRIBER_PID}" 2>/dev/null
}

start_tcp_server() {
    python3 -u "${PROJECT_DIR}/scripts/mock_tcp_server.py" \
        "${TCP_PORT}" >>"${TCP_RECEIVED_LOG}" 2>&1 &
    TCP_PID=$!
    wait_for_log_count "${TCP_RECEIVED_LOG}" \
        "TCP server listening" 1 5
}

start_socat() {
    rm -f -- "${STM32_DEVICE}" "${GATEWAY_DEVICE}"
    socat -d -d \
        "pty,raw,echo=0,link=${STM32_DEVICE}" \
        "pty,raw,echo=0,link=${GATEWAY_DEVICE}" \
        >>"${RAW_LOG}" 2>&1 &
    SOCAT_PID=$!

    for _ in {1..40}; do
        if [[ -e "${STM32_DEVICE}" && -e "${GATEWAY_DEVICE}" ]]; then
            return 0
        fi
        sleep 0.1
    done
    return 1
}

send_stream_case() {
    python3 "${PROJECT_DIR}/scripts/inject_stream_cases.py" \
        "${STM32_DEVICE}" --mode "$1" >>"${RAW_LOG}" 2>&1
}

send_bad_frame() {
    python3 "${PROJECT_DIR}/scripts/inject_bad_frames.py" \
        "${STM32_DEVICE}" "$1" >>"${RAW_LOG}" 2>&1
}

send_sticky_frames() {
    python3 "${PROJECT_DIR}/scripts/inject_sticky_frames.py" \
        "${STM32_DEVICE}" >>"${RAW_LOG}" 2>&1
}

preflight_ok=true
for dependency in \
    cmake python3 socat mosquitto mosquitto_pub \
    mosquitto_sub sqlite3; do
    require_command "${dependency}" || preflight_ok=false
done

if ! python3 -c 'import serial' >/dev/null 2>&1; then
    log "[ERROR] Python module 'serial' not found; install python3-serial or pyserial"
    preflight_ok=false
fi

if [[ "${preflight_ok}" != true ]]; then
    log "[FAIL] fault injection prerequisites are incomplete"
    exit 1
fi

log "[INFO] project=${PROJECT_DIR}"
log "[INFO] build=${BUILD_DIR}"

if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    log "[INFO] configuring project"
    if ! cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" >>"${RAW_LOG}" 2>&1; then
        log "[ERROR] CMake configuration failed"
        exit 1
    fi
fi

log "[INFO] building edge_gateway"
if ! cmake --build "${BUILD_DIR}" --target edge_gateway --parallel \
    >>"${RAW_LOG}" 2>&1; then
    log "[ERROR] edge_gateway build failed"
    exit 1
fi

GATEWAY_BIN="${BUILD_DIR}/edge_gateway"
if [[ ! -x "${GATEWAY_BIN}" ]]; then
    log "[ERROR] gateway executable not found: ${GATEWAY_BIN}"
    exit 1
fi

MQTT_PORT="$(find_free_port)"
TCP_PORT="$(find_free_port)"

cat >"${CONFIG_PATH}" <<EOF
serial:
  devices:
    - ${GATEWAY_DEVICE}
  baud_rate: 115200
  reconnect_interval_seconds: 1
mqtt:
  host: 127.0.0.1
  port: ${MQTT_PORT}
  topic_prefix: fi
  cache_retry_interval_seconds: 1
  username: ""
  password: ""
  tls:
    enabled: false
    ca_file: ""
    certificate_file: ""
    private_key_file: ""
    insecure: false
tcp:
  enabled: true
  host: 127.0.0.1
  port: ${TCP_PORT}
cache:
  type: sqlite
  path: ${CACHE_PATH}
log:
  path: ${GATEWAY_FILE_LOG}
  level: DEBUG
EOF

log "[INFO] starting isolated Mosquitto broker on port ${MQTT_PORT}"
start_broker || { log "[ERROR] Mosquitto broker did not start"; exit 1; }
start_subscriber || { log "[ERROR] MQTT subscriber did not start"; exit 1; }

log "[INFO] starting Python TCP server on port ${TCP_PORT}"
start_tcp_server || { log "[ERROR] TCP server did not start"; exit 1; }

log "[INFO] starting virtual serial pair"
start_socat || { log "[ERROR] socat virtual serial pair was not created"; exit 1; }

log "[INFO] starting edge_gateway"
"${GATEWAY_BIN}" "${CONFIG_PATH}" "${GATEWAY_DEVICE}" \
    >>"${RAW_LOG}" 2>&1 &
GATEWAY_PID=$!

if ! wait_for_log_count "${RAW_LOG}" "serial opened" 1 8 \
    || ! wait_for_log_count "${RAW_LOG}" "MQTT connected" 1 8; then
    log "[ERROR] gateway did not become ready"
    exit 1
fi

# FI-01: normal baseline.
before_mqtt="$(line_count "${MQTT_RECEIVED_LOG}")"
if send_stream_case normal \
    && wait_for_log_count "${MQTT_RECEIVED_LOG}" "fi/16/data" "$((before_mqtt + 1))" 5; then
    record_result FI-01 "正常帧基线" PASS "合法帧已解析并发布 JSON"
else
    record_result FI-01 "正常帧基线" FAIL "未观察到合法 MQTT 消息"
fi

# FI-02: bad CRC must not publish.
before_crc="$(pattern_count "${RAW_LOG}" "CRC validation failed")"
before_mqtt="$(line_count "${MQTT_RECEIVED_LOG}")"
send_bad_frame crc || true
sleep 1
after_mqtt="$(line_count "${MQTT_RECEIVED_LOG}")"
if wait_for_log_count "${RAW_LOG}" "CRC validation failed" "$((before_crc + 1))" 3 \
    && [[ "${after_mqtt}" == "${before_mqtt}" ]]; then
    record_result FI-02 "CRC 错误帧" PASS "协议异常已记录且未进入发布链路"
else
    record_result FI-02 "CRC 错误帧" FAIL "异常日志缺失或错误帧被发布"
fi

# FI-03: invalid length followed by a valid frame must recover.
before_length="$(pattern_count "${RAW_LOG}" "invalid payload length")"
before_overflow="$(pattern_count "${RAW_LOG}" "ring buffer overflow")"
before_mqtt="$(line_count "${MQTT_RECEIVED_LOG}")"
send_bad_frame length || true
wait_for_log_count "${RAW_LOG}" "invalid payload length" "$((before_length + 1))" 3 || true
send_stream_case normal || true
if wait_for_log_count "${MQTT_RECEIVED_LOG}" "fi/16/data" "$((before_mqtt + 1))" 5 \
    && [[ "$(pattern_count "${RAW_LOG}" "ring buffer overflow")" == "${before_overflow}" ]]; then
    record_result FI-03 "非法长度帧" PASS "非法长度已记录，解析器随后恢复且无缓冲区溢出"
else
    record_result FI-03 "非法长度帧" FAIL "解析器未恢复或出现缓冲区溢出"
fi

# FI-04: noise before a valid frame.
before_mqtt="$(line_count "${MQTT_RECEIVED_LOG}")"
if send_stream_case noise \
    && wait_for_log_count "${MQTT_RECEIVED_LOG}" "fi/16/data" "$((before_mqtt + 1))" 5; then
    record_result FI-04 "前置噪声" PASS "网关重新同步并解析后续合法帧"
else
    record_result FI-04 "前置噪声" FAIL "前置噪声后未恢复解析"
fi

# FI-05: split frame.
before_mqtt="$(line_count "${MQTT_RECEIVED_LOG}")"
if send_stream_case half \
    && wait_for_log_count "${MQTT_RECEIVED_LOG}" "fi/16/data" "$((before_mqtt + 1))" 5; then
    record_result FI-05 "半包" PASS "完整帧到达后发布一条消息"
else
    record_result FI-05 "半包" FAIL "拆分帧未被正确重组"
fi

# FI-06: two frames in one write.
before_mqtt="$(line_count "${MQTT_RECEIVED_LOG}")"
if send_sticky_frames \
    && wait_for_log_count "${MQTT_RECEIVED_LOG}" "fi/16/data" "$((before_mqtt + 2))" 5; then
    record_result FI-06 "粘包" PASS "一次写入中的两个合法帧均被解析"
else
    record_result FI-06 "粘包" FAIL "未连续发布两条消息"
fi

# FI-07: stop the owned broker and verify SQLite persistence.
terminate_process "${SUBSCRIBER_PID}"
SUBSCRIBER_PID=""
terminate_process "${BROKER_PID}"
BROKER_PID=""
wait_for_log_count "${RAW_LOG}" "MQTT connection lost" 1 5 || sleep 1
before_cache="$(cache_count)"
send_stream_case normal || true
if wait_for_cache_at_least "$((before_cache + 1))" 6; then
    record_result FI-07 "MQTT Broker 离线" PASS "发布失败后消息写入 SQLite 缓存"
else
    record_result FI-07 "MQTT Broker 离线" FAIL "SQLite 缓存未增加"
fi

# FI-08: restart the broker and verify replay.
before_connect="$(pattern_count "${RAW_LOG}" "MQTT connected")"
if start_broker && start_subscriber \
    && wait_for_log_count "${RAW_LOG}" "MQTT connected" "$((before_connect + 1))" 8 \
    && wait_for_cache_zero 10; then
    record_result FI-08 "MQTT Broker 恢复" PASS "SQLite 队列已补传并清空"
else
    record_result FI-08 "MQTT Broker 恢复" FAIL "Broker 未恢复或缓存未清空"
fi

# FI-09: TCP failure must not block MQTT.
terminate_process "${TCP_PID}"
TCP_PID=""
before_mqtt="$(line_count "${MQTT_RECEIVED_LOG}")"
if send_stream_case normal \
    && wait_for_log_count "${MQTT_RECEIVED_LOG}" "fi/16/data" "$((before_mqtt + 1))" 5 \
    && kill -0 "${GATEWAY_PID}" 2>/dev/null; then
    record_result FI-09 "TCP Server 离线" PASS "TCP 故障未阻塞 MQTT 通道"
else
    record_result FI-09 "TCP Server 离线" FAIL "MQTT 发布受阻或网关退出"
fi

# FI-10: remove and recreate the PTY pair.
before_lost="$(pattern_count "${RAW_LOG}" "connection lost")"
before_restored="$(pattern_count "${RAW_LOG}" "serial connection restored")"
terminate_process "${SOCAT_PID}"
SOCAT_PID=""
wait_for_log_count "${RAW_LOG}" "connection lost" "$((before_lost + 1))" 6 || true

if start_socat \
    && wait_for_log_count "${RAW_LOG}" "serial connection restored" "$((before_restored + 1))" 8; then
    before_mqtt="$(line_count "${MQTT_RECEIVED_LOG}")"
    if send_stream_case normal \
        && wait_for_log_count "${MQTT_RECEIVED_LOG}" "fi/16/data" "$((before_mqtt + 1))" 5; then
        record_result FI-10 "串口断开与恢复" PASS "网关保持运行并在重连后继续解析"
    else
        record_result FI-10 "串口断开与恢复" FAIL "重连后未解析合法帧"
    fi
else
    record_result FI-10 "串口断开与恢复" FAIL "虚拟串口或网关重连失败"
fi

# FI-11: SIGTERM should produce a normal exit path.
kill -TERM "${GATEWAY_PID}" 2>/dev/null || true
wait "${GATEWAY_PID}" 2>/dev/null
gateway_exit_code=$?
GATEWAY_PID=""

if [[ ${gateway_exit_code} -eq 0 ]] \
    && grep -Fq "shutdown requested" "${RAW_LOG}" \
    && grep -Fq "edge gateway stopped" "${RAW_LOG}"; then
    record_result FI-11 "SIGTERM 优雅退出" PASS "退出码为 0 且记录完整退出路径"
else
    record_result FI-11 "SIGTERM 优雅退出" FAIL "退出码=${gateway_exit_code} 或退出日志不完整"
fi

{
    echo
    echo "===== MQTT RECEIVED ====="
    cat "${MQTT_RECEIVED_LOG}" 2>/dev/null || true
    echo
    echo "===== TCP SERVER ====="
    cat "${TCP_RECEIVED_LOG}" 2>/dev/null || true
    echo
    echo "===== GATEWAY FILE LOG ====="
    cat "${GATEWAY_FILE_LOG}" 2>/dev/null || true
} >>"${RAW_LOG}"

git_commit="$(git -C "${PROJECT_DIR}" rev-parse HEAD 2>/dev/null || echo unknown)"
display_build_dir="${BUILD_DIR/#${HOME}/<home>}"
test_end="$(date --iso-8601=seconds 2>/dev/null || date)"
failure_count=0

{
    echo "# Fault Injection Summary"
    echo
    echo "- Test start: ${TEST_START}"
    echo "- Test end: ${test_end}"
    echo "- Git commit: \`${git_commit}\`"
    echo "- Build directory: \`${display_build_dir}\`"
    echo "- Serial source: socat virtual PTY pair"
    echo
    echo "## Results"
    echo
    echo "| ID | Scenario | Result | Detail |"
    echo "| --- | --- | --- | --- |"
    for index in "${!SCENARIO_IDS[@]}"; do
        echo "| ${SCENARIO_IDS[$index]} | ${SCENARIO_NAMES[$index]} | ${SCENARIO_RESULTS[$index]} | ${SCENARIO_DETAILS[$index]} |"
        if [[ "${SCENARIO_RESULTS[$index]}" != "PASS" ]]; then
            ((failure_count += 1))
        fi
    done
    echo
    echo "## Failure Log Summary"
    echo
    if (( failure_count == 0 )); then
        echo "No failed scenarios."
    else
        echo '```text'
        tail -n 60 "${RAW_LOG}"
        echo '```'
    fi
    echo
    echo "## Engineering Boundary"
    echo
    echo "This suite performs software fault injection in a virtual serial and public reproducibility environment. It does not represent industrial field EMC, power disturbance, extreme network conditions, or mass-production reliability. It does not establish fixed throughput, P99 latency, or continuous production availability."
} >"${SUMMARY_PATH}"

printf '\n%-7s %-28s %-6s\n' "ID" "SCENARIO" "RESULT"
printf '%-7s %-28s %-6s\n' "-------" "----------------------------" "------"
for index in "${!SCENARIO_IDS[@]}"; do
    printf '%-7s %-28s %-6s\n' \
        "${SCENARIO_IDS[$index]}" \
        "${SCENARIO_NAMES[$index]}" \
        "${SCENARIO_RESULTS[$index]}"
done

log "[INFO] summary=${SUMMARY_PATH}"
log "[INFO] raw_log=${RAW_LOG}"

if (( failure_count > 0 )); then
    log "[FAIL] ${failure_count} fault injection scenario(s) failed"
    exit 1
fi

log "[PASS] all fault injection scenarios passed"
exit 0
