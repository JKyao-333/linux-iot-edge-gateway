#!/usr/bin/env bash

set -u

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTIFACT_DIR="${PROJECT_DIR}/artifacts"
SUMMARY_PATH="${ARTIFACT_DIR}/validation_workflow_check.md"
TMP_DIR="$(mktemp -d /tmp/gateway-validation-workflow.XXXXXX)" || {
    echo "[ERROR] cannot create temporary directory" >&2
    exit 1
}

trap 'rm -rf -- "${TMP_DIR}"' EXIT

mkdir -p -- "${ARTIFACT_DIR}" || {
    echo "[ERROR] cannot create artifacts directory" >&2
    exit 1
}

RESULT_IDS=()
RESULT_LABELS=()
RESULT_STATUSES=()
RESULT_DETAILS=()
FAILED=0

record_result() {
    RESULT_IDS+=("$1")
    RESULT_LABELS+=("$2")
    RESULT_STATUSES+=("$3")
    RESULT_DETAILS+=("$4")
}

run_required_check() {
    local check_id="$1"
    local label="$2"
    shift 2
    local output_file="${TMP_DIR}/${check_id}.log"

    if "$@" >"${output_file}" 2>&1; then
        record_result "${check_id}" "${label}" "PASS" "exit code 0"
        echo "[PASS] ${label}"
    else
        local exit_code=$?
        record_result "${check_id}" "${label}" "FAIL" "exit code ${exit_code}"
        echo "[FAIL] ${label}, exit_code=${exit_code}" >&2
        tail -n 8 -- "${output_file}" >&2 || true
        FAILED=1
    fi
}

if ! command -v python3 >/dev/null 2>&1; then
    echo "[ERROR] python3 is required" >&2
    exit 1
fi

run_required_check \
    replay \
    "Serial replay dry-run" \
    python3 "${PROJECT_DIR}/scripts/serial_replay.py" \
        --input "${PROJECT_DIR}/data/test_frames/valid_frames.hex" \
        --serial /tmp/tty_dummy \
        --dry-run

run_required_check \
    sanitize \
    "Log sanitization dry-run" \
    python3 "${PROJECT_DIR}/scripts/sanitize_logs.py" \
        --input "${PROJECT_DIR}/logs/sample_gateway.log" \
        --dry-run

HEALTH_LOG="${TMP_DIR}/health.log"
"${PROJECT_DIR}/scripts/check_gateway_health.sh" \
    --service linux-iot-edge-gateway \
    --process-name edge_gateway \
    --output "${ARTIFACT_DIR}/health_check.md" \
    >"${HEALTH_LOG}" 2>&1
HEALTH_EXIT=$?
if (( HEALTH_EXIT <= 2 )); then
    record_result \
        health \
        "Gateway health observation" \
        "OBSERVED" \
        "exit code ${HEALTH_EXIT}; inactive service or process is allowed"
    echo "[INFO] Gateway health observation completed, exit_code=${HEALTH_EXIT}"
else
    record_result \
        health \
        "Gateway health observation" \
        "FAIL" \
        "script error, exit code ${HEALTH_EXIT}"
    echo "[FAIL] Gateway health observation, exit_code=${HEALTH_EXIT}" >&2
    tail -n 8 -- "${HEALTH_LOG}" >&2 || true
    FAILED=1
fi

run_required_check \
    environment \
    "Environment capture" \
    "${PROJECT_DIR}/scripts/collect_env.sh" \
        --include-git \
        --output "${ARTIFACT_DIR}/env_report.md"

run_required_check \
    observability \
    "HTTP observability test" \
    "${PROJECT_DIR}/scripts/run_observability_test.sh"

OVERALL="PASS"
(( FAILED == 0 )) || OVERALL="FAIL"
COLLECTED_AT="$(date --iso-8601=seconds 2>/dev/null || date)"
GIT_COMMIT="$(git -C "${PROJECT_DIR}" rev-parse HEAD 2>/dev/null || echo unknown)"

{
    echo "# Validation Workflow Check"
    echo
    echo "- Collected at: ${COLLECTED_AT}"
    echo "- Git commit: \`${GIT_COMMIT}\`"
    echo "- Overall: **${OVERALL}**"
    echo
    echo "| Check | Status | Detail |"
    echo "| --- | --- | --- |"
    for index in "${!RESULT_IDS[@]}"; do
        echo "| ${RESULT_LABELS[$index]} | ${RESULT_STATUSES[$index]} | ${RESULT_DETAILS[$index]} |"
    done
    echo
    echo "The health check is observational in this lightweight workflow. Exit codes 1 and 2 can indicate that the target service or process is not currently running."
    echo
    echo "This check validates the engineering toolchain, including HTTP observability. It does not establish fixed throughput, latency, long-duration availability, or industrial field reliability."
} >"${SUMMARY_PATH}" || {
    echo "[ERROR] cannot write workflow summary" >&2
    exit 1
}

if (( FAILED != 0 )); then
    echo "[FAIL] validation workflow check failed, report=artifacts/validation_workflow_check.md" >&2
    exit 1
fi

echo "[PASS] validation workflow check passed, report=artifacts/validation_workflow_check.md"
exit 0
