#!/usr/bin/env bash

set -u

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_PATH="${PROJECT_DIR}/artifacts/env_report.md"
INCLUDE_SERIAL=false
INCLUDE_NETWORK=false
INCLUDE_GIT=false
INCLUDE_PACKAGES=false
HASH_FILES=()

usage() {
    cat <<'EOF'
Usage: collect_env.sh [options]

Options:
  --output <path>       Markdown output path
  --include-serial      Include redacted serial device counts
  --include-network     Include redacted local network addresses
  --include-git         Include extended Git metadata
  --include-packages    Include selected package versions
  --include-hash <file> Include a SHA-256 file hash (repeatable)
  -h, --help            Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output)
            [[ $# -ge 2 ]] || { echo "[ERROR] --output requires a path"; exit 2; }
            OUTPUT_PATH="$2"
            shift 2
            ;;
        --include-serial)
            INCLUDE_SERIAL=true
            shift
            ;;
        --include-network)
            INCLUDE_NETWORK=true
            shift
            ;;
        --include-git)
            INCLUDE_GIT=true
            shift
            ;;
        --include-packages)
            INCLUDE_PACKAGES=true
            shift
            ;;
        --include-hash)
            [[ $# -ge 2 ]] || { echo "[ERROR] --include-hash requires a file"; exit 2; }
            HASH_FILES+=("$2")
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "[ERROR] unknown option: $1"
            usage
            exit 2
            ;;
    esac
done

OUTPUT_DIR="$(dirname "${OUTPUT_PATH}")"
if ! mkdir -p -- "${OUTPUT_DIR}" 2>/dev/null; then
    echo "[ERROR] cannot create output directory: ${OUTPUT_DIR}"
    exit 1
fi

if ! : >"${OUTPUT_PATH}" 2>/dev/null; then
    echo "[ERROR] output path is not writable: ${OUTPUT_PATH}"
    exit 1
fi

command_first_line() {
    local command_name="$1"
    shift

    if ! command -v "${command_name}" >/dev/null 2>&1; then
        printf '%s' "not found"
        return
    fi

    "$@" 2>&1 | head -n 1 || true
}

sanitize_text() {
    local user_name="${USER:-}"
    local expressions=(
        -e "s#${PROJECT_DIR//\#/\\#}#<repo-root>#g"
        -e "s#${HOME//\#/\\#}#<home>#g"
    )

    if [[ -n "${user_name}" ]]; then
        expressions+=(
            -e "s#${user_name//\#/\\#}#<redacted-user>#g"
        )
    fi

    sed "${expressions[@]}"
}

write_block() {
    local command_output="$1"
    printf '```text\n%s\n```\n\n' "${command_output}" >>"${OUTPUT_PATH}"
}

git_commit="not a Git worktree"
git_clean="not available"
git_branch="not available"

if git -C "${PROJECT_DIR}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git_commit="$(git -C "${PROJECT_DIR}" rev-parse HEAD 2>/dev/null || echo unknown)"
    if [[ -z "$(git -C "${PROJECT_DIR}" status --porcelain 2>/dev/null)" ]]; then
        git_clean="clean"
    else
        git_clean="dirty"
    fi
    git_branch="$(git -C "${PROJECT_DIR}" branch --show-current 2>/dev/null || echo unknown)"
fi

os_release="not found"
[[ -r /etc/os-release ]] && os_release="$(cat /etc/os-release | sanitize_text)"

cpu_summary="not found"
if command -v lscpu >/dev/null 2>&1; then
    cpu_summary="$(lscpu 2>/dev/null | grep -E '^(Architecture|CPU\(s\)|Model name|Thread\(s\) per core|Core\(s\) per socket|Socket\(s\)):' || true)"
fi

memory_summary="not found"
command -v free >/dev/null 2>&1 && memory_summary="$(free -h 2>/dev/null || true)"

disk_summary="not found"
command -v df >/dev/null 2>&1 && disk_summary="$(df -h "${PROJECT_DIR}" 2>/dev/null | sanitize_text || true)"

{
    echo "# Environment Report"
    echo
    echo "## Summary"
    echo
    echo "- Collected at: $(date --iso-8601=seconds 2>/dev/null || date)"
    echo "- Repository: \`<repo-root>\`"
    echo "- Current user: \`<redacted-user>\` (collected and redacted by default)"
    echo "- Shell: \`$(basename "${SHELL:-unknown}")\`"
    echo
    echo "## Git"
    echo
    echo "- Commit: \`${git_commit}\`"
    echo "- Worktree: \`${git_clean}\`"
    if [[ "${INCLUDE_GIT}" == true ]]; then
        echo "- Branch: \`${git_branch}\`"
        remote_count="$(git -C "${PROJECT_DIR}" remote 2>/dev/null | wc -l | xargs)"
        echo "- Configured remotes: \`${remote_count:-0}\` (URLs omitted)"
    else
        echo "- Extended Git metadata: not requested"
    fi
    echo
    echo "## OS"
    echo
    echo "### uname"
    echo
} >>"${OUTPUT_PATH}"

write_block "$(uname -a 2>/dev/null | sanitize_text || echo 'not found')"

{
    echo "### /etc/os-release"
    echo
} >>"${OUTPUT_PATH}"
write_block "${os_release}"

{
    echo "## CPU / Memory"
    echo
    echo "### CPU"
    echo
} >>"${OUTPUT_PATH}"
write_block "${cpu_summary}"

{
    echo "### Memory"
    echo
} >>"${OUTPUT_PATH}"
write_block "${memory_summary}"

{
    echo "### Disk"
    echo
} >>"${OUTPUT_PATH}"
write_block "${disk_summary}"

{
    echo "## Toolchain"
    echo
    echo "| Tool | Version / Status |"
    echo "| --- | --- |"
    echo "| gcc | $(command_first_line gcc gcc --version) |"
    echo "| g++ | $(command_first_line g++ g++ --version) |"
    echo "| cmake | $(command_first_line cmake cmake --version) |"
    echo "| python3 | $(command_first_line python3 python3 --version) |"
    echo "| qemu-aarch64 | $(command_first_line qemu-aarch64 qemu-aarch64 --version) |"
    echo "| aarch64-linux-gnu-g++ | $(command_first_line aarch64-linux-gnu-g++ aarch64-linux-gnu-g++ --version) |"
    echo
    echo "## Runtime Dependencies"
    echo
    echo "| Dependency | Version / Status |"
    echo "| --- | --- |"
    echo "| sqlite3 | $(command_first_line sqlite3 sqlite3 --version) |"
    echo "| mosquitto | $(command_first_line mosquitto mosquitto --help) |"
    echo "| mosquitto_sub | $(command_first_line mosquitto_sub mosquitto_sub --help) |"
    echo "| mosquitto_pub | $(command_first_line mosquitto_pub mosquitto_pub --help) |"
    echo "| pkg-config | $(command_first_line pkg-config pkg-config --version) |"
} >>"${OUTPUT_PATH}"

if command -v pkg-config >/dev/null 2>&1 \
    && pkg-config --exists libmosquitto 2>/dev/null; then
    echo "| libmosquitto | detected ($(pkg-config --modversion libmosquitto 2>/dev/null || echo unknown)) |" >>"${OUTPUT_PATH}"
else
    echo "| libmosquitto | not found by pkg-config |" >>"${OUTPUT_PATH}"
fi

if command -v systemctl >/dev/null 2>&1; then
    if [[ "$(cat /proc/1/comm 2>/dev/null || true)" == "systemd" ]]; then
        echo "| systemd | available and running as PID 1 |" >>"${OUTPUT_PATH}"
    else
        echo "| systemd | command available; not running as PID 1 |" >>"${OUTPUT_PATH}"
    fi
else
    echo "| systemd | not found |" >>"${OUTPUT_PATH}"
fi
echo >>"${OUTPUT_PATH}"

{
    echo "## Serial Devices"
    echo
    if [[ "${INCLUDE_SERIAL}" == true ]]; then
        shopt -s nullglob
        tty_usb=(/dev/ttyUSB*)
        tty_acm=(/dev/ttyACM*)
        serial_ids=(/dev/serial/by-id/*)
        shopt -u nullglob
        echo "- ttyUSB device count: ${#tty_usb[@]}"
        echo "- ttyACM device count: ${#tty_acm[@]}"
        echo "- serial/by-id entry count: ${#serial_ids[@]}"
        echo "- Device names and serial identifiers are redacted."
    else
        echo "Serial collection not requested. Use \`--include-serial\`."
    fi
    echo
    echo "## Network"
    echo
    if [[ "${INCLUDE_NETWORK}" == true ]]; then
        if command -v ip >/dev/null 2>&1; then
            echo "Redacted local addresses:"
            ip -o -4 addr show scope global 2>/dev/null \
                | awk '{print $4}' \
                | sed -E 's#^([0-9]+\.[0-9]+)\.[0-9]+\.[0-9]+/([0-9]+)$#\1.x.x/\2#' \
                | sed 's/^/- /' || true
            if ip -o -6 addr show scope global 2>/dev/null | grep -q .; then
                echo "- <redacted-ipv6>"
            fi
        else
            echo "- ip command: not found"
        fi
        echo "- Wi-Fi names, MAC addresses, public IPs, and credentials are not collected."
    else
        echo "Network collection not requested. Use \`--include-network\`."
    fi
    echo
    echo "## File Hashes"
    echo
} >>"${OUTPUT_PATH}"

if [[ ${#HASH_FILES[@]} -eq 0 ]]; then
    echo "No files requested." >>"${OUTPUT_PATH}"
else
    echo "| File | SHA-256 |" >>"${OUTPUT_PATH}"
    echo "| --- | --- |" >>"${OUTPUT_PATH}"
    for hash_file in "${HASH_FILES[@]}"; do
        hash_label="$(basename "${hash_file}")"
        if [[ ! -f "${hash_file}" ]]; then
            echo "| \`${hash_label}\` | not found |" >>"${OUTPUT_PATH}"
        elif command -v sha256sum >/dev/null 2>&1; then
            hash_value="$(sha256sum "${hash_file}" | awk '{print $1}')"
            echo "| \`${hash_label}\` | \`${hash_value}\` |" >>"${OUTPUT_PATH}"
        else
            echo "| \`${hash_label}\` | sha256sum not found |" >>"${OUTPUT_PATH}"
        fi
    done
fi
echo >>"${OUTPUT_PATH}"

if [[ "${INCLUDE_PACKAGES}" == true ]]; then
    {
        echo "## Selected Packages"
        echo
        if command -v dpkg-query >/dev/null 2>&1; then
            echo '```text'
            for package_name in \
                cmake g++ mosquitto mosquitto-clients \
                libmosquitto-dev libsqlite3-dev sqlite3 \
                socat python3 python3-serial libyaml-cpp-dev; do
                package_version="$(dpkg-query -W -f='${Version}' "${package_name}" 2>/dev/null || true)"
                printf '%-24s %s\n' "${package_name}" "${package_version:-not installed}"
            done
            echo '```'
        else
            echo "dpkg-query not found; package inventory unavailable."
        fi
        echo
    } >>"${OUTPUT_PATH}"
fi

{
    echo "## Notes / Redaction Policy"
    echo
    echo "- Repository and home paths are replaced with \`<repo-root>\` and \`<home>\`."
    echo "- The current account name is replaced with \`<redacted-user>\`."
    echo "- Serial numbers, MAC addresses, Wi-Fi names, public IPs, and credentials are not collected."
    echo "- Missing commands are recorded as \`not found\` and do not make collection fail."
    echo "- This report captures per-run environment fields and complements the public reproducibility evidence ledger."
} >>"${OUTPUT_PATH}"

echo "[PASS] environment report written: ${OUTPUT_PATH}"
exit 0
