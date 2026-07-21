#!/usr/bin/env bash

set -euo pipefail

DEFAULT_ROOT="${HOME}/Tools/linux-iot-edge-gateway"
TOOLS_ROOT="${1:-${AARCH64_TOOLS_ROOT:-${DEFAULT_ROOT}}}"
SYSROOT="${TOOLS_ROOT}/aarch64-sysroot"
PACKAGE_DIR="${TOOLS_ROOT}/aarch64-packages"
RUNTIME_DIR="$(mktemp -d /tmp/iot-aarch64-setup.XXXXXX)"

cleanup() {
    rm -rf -- "${RUNTIME_DIR}"
}

trap cleanup EXIT INT TERM

if [[ ! -d /mnt/d ]] && [[ "${TOOLS_ROOT}" == /mnt/d/* ]]; then
    echo "[ERROR] D drive is not mounted at /mnt/d"
    exit 1
fi

if ! command -v aarch64-linux-gnu-g++ >/dev/null 2>&1; then
    echo "[INFO] installing GNU AArch64 cross compiler"
    sudo apt-get update
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
        g++-aarch64-linux-gnu \
        file \
        pkg-config
fi

mkdir -p \
    "${PACKAGE_DIR}" \
    "${RUNTIME_DIR}/lists/partial" \
    "${RUNTIME_DIR}/cache/archives/partial"

chmod 755 \
    "${RUNTIME_DIR}" \
    "${RUNTIME_DIR}/lists" \
    "${RUNTIME_DIR}/cache" \
    "${RUNTIME_DIR}/cache/archives"

cat >"${RUNTIME_DIR}/sources.list" <<'EOF'
deb [arch=arm64 signed-by=/usr/share/keyrings/ubuntu-archive-keyring.gpg] http://ports.ubuntu.com/ubuntu-ports jammy main universe
deb [arch=arm64 signed-by=/usr/share/keyrings/ubuntu-archive-keyring.gpg] http://ports.ubuntu.com/ubuntu-ports jammy-updates main universe
deb [arch=arm64 signed-by=/usr/share/keyrings/ubuntu-archive-keyring.gpg] http://ports.ubuntu.com/ubuntu-ports jammy-security main universe
EOF

APT_OPTIONS=(
    -o "Dir::State::lists=${RUNTIME_DIR}/lists"
    -o "Dir::Cache=${RUNTIME_DIR}/cache"
    -o "Dir::Etc::sourcelist=${RUNTIME_DIR}/sources.list"
    -o "Dir::Etc::sourceparts=-"
    -o "APT::Architecture=arm64"
)

PACKAGES=(
    libyaml-cpp-dev:arm64
    libyaml-cpp0.7:arm64
    libsqlite3-dev:arm64
    libsqlite3-0:arm64
    libmosquitto-dev:arm64
    libmosquitto1:arm64
    libssl3:arm64
)

echo "[INFO] refreshing Ubuntu ARM64 package metadata"
apt-get "${APT_OPTIONS[@]}" update

echo "[INFO] downloading ARM64 dependencies to ${PACKAGE_DIR}"
rm -f -- "${PACKAGE_DIR}"/*.deb
(
    cd "${PACKAGE_DIR}"
    apt-get "${APT_OPTIONS[@]}" download "${PACKAGES[@]}"
)

echo "[INFO] extracting ARM64 dependency sysroot"
rm -rf -- "${SYSROOT}"
mkdir -p "${SYSROOT}"

for package_file in "${PACKAGE_DIR}"/*.deb; do
    dpkg-deb -x "${package_file}" "${SYSROOT}"
done

required_files=(
    "usr/include/mosquitto.h"
    "usr/include/sqlite3.h"
    "usr/include/yaml-cpp/yaml.h"
    "usr/lib/aarch64-linux-gnu/libmosquitto.so"
    "usr/lib/aarch64-linux-gnu/libsqlite3.so"
    "usr/lib/aarch64-linux-gnu/libyaml-cpp.so"
)

for required_file in "${required_files[@]}"; do
    if [[ ! -e "${SYSROOT}/${required_file}" ]]; then
        echo "[ERROR] incomplete ARM64 sysroot: ${required_file} is missing"
        exit 1
    fi
done

{
    echo "architecture=aarch64"
    echo "ubuntu_release=22.04"
    echo "generated_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf 'package=%s\n' "${PACKAGES[@]}"
} >"${SYSROOT}/gateway-sysroot.manifest"

echo "[PASS] ARM64 sysroot ready: ${SYSROOT}"
aarch64-linux-gnu-g++ --version | head -n 1
