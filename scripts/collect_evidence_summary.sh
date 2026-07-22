#!/usr/bin/env bash

set -u

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_PATH="${PROJECT_DIR}/artifacts/evidence_summary.md"

warn() {
    printf '[WARN] %s\n' "$1" >&2
}

sanitize_text() {
    local text="$1"
    local host_name=""
    local user_name="${USER:-}"

    if command -v hostname >/dev/null 2>&1; then
        host_name="$(hostname 2>/dev/null || true)"
    fi

    text="${text//${PROJECT_DIR}/<repo-root>}"
    if [[ -n "${HOME:-}" ]]; then
        text="${text//${HOME}/<home>}"
    fi
    if [[ -n "${user_name}" ]]; then
        text="${text//${user_name}/<redacted-user>}"
    fi
    if [[ -n "${host_name}" ]]; then
        text="${text//${host_name}/<redacted-host>}"
    fi

    printf '%s' "${text}" |
        sed -E \
            -e 's#/home/[^/[:space:]]+#<home>#g' \
            -e 's#/mnt/[[:alpha:]]/Users/[^/[:space:]]+#<windows-home>#g'
}

relative_artifact_path() {
    local absolute_path="$1"

    if [[ "${absolute_path}" == "${PROJECT_DIR}/"* ]]; then
        printf '%s' "${absolute_path#${PROJECT_DIR}/}"
    else
        printf '%s' "not found"
    fi
}

latest_artifact() {
    local file_name="$1"
    local latest=""

    latest="$(find "${PROJECT_DIR}/artifacts" -type f -name "${file_name}" \
        -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -n 1 | cut -d' ' -f2- || true)"

    if [[ -n "${latest}" ]]; then
        relative_artifact_path "${latest}"
    else
        printf '%s' "not found"
    fi
}

command_status() {
    local command_name="$1"

    if command -v "${command_name}" >/dev/null 2>&1; then
        printf '%s' "available"
    else
        warn "optional command not found: ${command_name}"
        printf '%s' "not found"
    fi
}

mkdir -p "${PROJECT_DIR}/artifacts" || {
    echo "[ERROR] cannot create artifacts directory" >&2
    exit 1
}

if ! : >"${OUTPUT_PATH}" 2>/dev/null; then
    echo "[ERROR] cannot write evidence summary" >&2
    exit 1
fi

collected_at="$(date --iso-8601=seconds 2>/dev/null || date)"
git_commit="$(git -C "${PROJECT_DIR}" rev-parse HEAD 2>/dev/null || printf 'not available')"
uname_summary="$(uname -a 2>/dev/null || printf 'not found')"
uname_summary="$(sanitize_text "${uname_summary}")"

os_summary="not found"
if [[ -r /etc/os-release ]]; then
    os_summary="$(grep -E '^(PRETTY_NAME|ID|VERSION_ID)=' /etc/os-release 2>/dev/null | tr '\n' '; ' | sed 's/; $//' || true)"
fi

tty_usb_count="$(compgen -G '/dev/ttyUSB*' | wc -l)"
tty_acm_count="$(compgen -G '/dev/ttyACM*' | wc -l)"
tty_gateway_count="$(compgen -G '/tmp/tty_gateway*' | wc -l)"
tty_stm32_count="$(compgen -G '/tmp/tty_stm32*' | wc -l)"

mqtt_status="not checked"
if command -v mosquitto_sub >/dev/null 2>&1; then
    if mosquitto_sub -h localhost -p 1883 -t '$SYS/broker/version' -C 1 -W 2 >/dev/null 2>&1; then
        mqtt_status="reachable on localhost:1883"
    else
        mqtt_status="not reachable on localhost:1883"
        warn "MQTT broker is not reachable on localhost:1883"
    fi
else
    warn "optional command not found: mosquitto_sub"
fi

docker_status="$(command_status docker)"
if [[ "${docker_status}" == "available" ]]; then
    if docker info >/dev/null 2>&1; then
        docker_status="CLI and daemon available"
    else
        docker_status="CLI available; daemon unavailable"
        warn "Docker CLI is available but the daemon is unavailable"
    fi
fi

vcan_status="not checked"
if command -v ip >/dev/null 2>&1; then
    if ip link show vcan0 >/dev/null 2>&1; then
        vcan_status="present"
    else
        vcan_status="not present"
    fi
else
    warn "optional command not found: ip"
fi

stability_summary="$(latest_artifact stability_summary.md)"
fault_summary="$(latest_artifact fault_injection_summary.md)"
observability_summary="$(latest_artifact observability_test_summary.md)"
workflow_summary="$(latest_artifact validation_workflow_check.md)"

cat >"${OUTPUT_PATH}" <<EOF
# Local Reproducibility Evidence Summary

## Collection

| Field | Value |
| --- | --- |
| collected_at | ${collected_at} |
| git commit | ${git_commit} |
| current user | \`<redacted-user>\` |
| repository | \`<repo-root>\` |

## Operating System

| Field | Value |
| --- | --- |
| uname -a | \`${uname_summary}\` |
| os-release | \`${os_summary}\` |

## Device And Runtime Summary

| Field | Value |
| --- | --- |
| TTY USB devices | ${tty_usb_count} device(s), identifiers not collected |
| TTY ACM devices | ${tty_acm_count} device(s), identifiers not collected |
| virtual gateway PTYs | ${tty_gateway_count} device(s), paths not listed |
| virtual STM32 PTYs | ${tty_stm32_count} device(s), paths not listed |
| MQTT broker | ${mqtt_status} |
| Docker | ${docker_status} |
| vcan0 | ${vcan_status} |

## Local Artifact Index

| Evidence | Latest local path |
| --- | --- |
| Stability summary | \`${stability_summary}\` |
| Fault injection summary | \`${fault_summary}\` |
| Observability summary | \`${observability_summary}\` |
| Validation workflow check | \`${workflow_summary}\` |

## Repository Evidence Index

- References: \`docs/references.md\`
- Public reproducibility evidence ledger: \`docs/reproducible_evidence_ledger.md\`

## Redaction Policy

- Local repository, home, user, and host identifiers are redacted.
- Serial device identifiers and USB serial numbers are not collected.
- Public IP addresses, Wi-Fi names, MAC addresses, tokens, private keys, and certificate contents are not collected.
- Local artifact paths are reported only relative to \`<repo-root>\`.
- This generated report is a local runtime artifact and is ignored by Git.
EOF

echo "[PASS] evidence summary written: <repo-root>/artifacts/evidence_summary.md"
exit 0
