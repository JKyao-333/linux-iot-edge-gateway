#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

DEFAULT_TOOLS_ROOT="/mnt/d/Tools/linux-iot-edge-gateway"
TOOLS_ROOT="${AARCH64_TOOLS_ROOT:-${DEFAULT_TOOLS_ROOT}}"
SYSROOT="${AARCH64_SYSROOT:-${TOOLS_ROOT}/aarch64-sysroot}"
BUILD_DIR="${AARCH64_BUILD_DIR:-${HOME}/linux-iot-edge-gateway-build-aarch64}"
OUTPUT_DIR="${AARCH64_OUTPUT_DIR:-${TOOLS_ROOT}/artifacts}"
STAGE_DIR="${BUILD_DIR}/stage"
VERSION="$("${PROJECT_DIR}/scripts/verify_release_version.sh")"
ARTIFACT_NAME="linux-iot-edge-gateway-${VERSION}-aarch64.tar.gz"
ARTIFACT_PATH="${OUTPUT_DIR}/${ARTIFACT_NAME}"
CHECKSUM_PATH="${ARTIFACT_PATH}.sha256"

for command_name in \
    aarch64-linux-gnu-g++ \
    cmake \
    file \
    readelf \
    sha256sum \
    tar; do

    if ! command -v "${command_name}" >/dev/null 2>&1; then
        echo "[ERROR] required command not found: ${command_name}"
        echo "[INFO] run scripts/setup_aarch64_sysroot.sh first"
        exit 1
    fi
done

if [[ ! -f "${SYSROOT}/gateway-sysroot.manifest" ]]; then
    echo "[ERROR] ARM64 sysroot is not initialized: ${SYSROOT}"
    echo "[INFO] run scripts/setup_aarch64_sysroot.sh first"
    exit 1
fi

echo "[INFO] configuring AArch64 build"
cmake \
    -S "${PROJECT_DIR}" \
    -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${PROJECT_DIR}/cmake/toolchains/aarch64-linux-gnu.cmake" \
    -DAARCH64_SYSROOT="${SYSROOT}"

echo "[INFO] building ARM64 edge_gateway"
cmake \
    --build "${BUILD_DIR}" \
    --target edge_gateway \
    --parallel

BINARY_PATH="${BUILD_DIR}/edge_gateway"

if ! file "${BINARY_PATH}" | grep -q 'ARM aarch64'; then
    echo "[ERROR] build output is not an AArch64 ELF"
    file "${BINARY_PATH}"
    exit 1
fi

if ! readelf -h "${BINARY_PATH}" \
    | grep -Eq 'Machine:[[:space:]]+AArch64'; then

    echo "[ERROR] readelf did not report AArch64"
    exit 1
fi

if command -v qemu-aarch64 >/dev/null 2>&1; then
    QEMU_LOG="${BUILD_DIR}/qemu-startup.log"

    QEMU_VERSION="$(
        qemu-aarch64 \
            -L /usr/aarch64-linux-gnu \
            -E "LD_LIBRARY_PATH=${SYSROOT}/usr/lib/aarch64-linux-gnu" \
            "${BINARY_PATH}" \
            --version
    )"

    if [[ "${QEMU_VERSION}" != "linux-iot-edge-gateway ${VERSION}" ]]; then
        echo "[ERROR] ARM64 version output does not match VERSION"
        echo "[INFO] actual: ${QEMU_VERSION}"
        exit 1
    fi

    echo "[PASS] ARM64 version: ${QEMU_VERSION}"

    set +e
    qemu-aarch64 \
        -L /usr/aarch64-linux-gnu \
        -E "LD_LIBRARY_PATH=${SYSROOT}/usr/lib/aarch64-linux-gnu" \
        "${BINARY_PATH}" \
        "${BUILD_DIR}/missing-config.yaml" \
        >"${QEMU_LOG}" 2>&1
    qemu_status=$?
    set -e

    if [[ ${qemu_status} -ne 1 ]] \
        || ! grep -Fq 'load config failed:' "${QEMU_LOG}"; then

        echo "[ERROR] ARM64 QEMU startup verification failed"
        cat "${QEMU_LOG}"
        exit 1
    fi

    echo "[PASS] ARM64 binary started successfully under QEMU"
else
    echo "[INFO] qemu-aarch64 not installed; startup verification skipped"
fi

rm -rf -- "${STAGE_DIR}"
mkdir -p \
    "${STAGE_DIR}/usr/local/bin" \
    "${STAGE_DIR}/usr/local/share/linux-iot-edge-gateway" \
    "${STAGE_DIR}/etc/linux-iot-edge-gateway" \
    "${STAGE_DIR}/lib/systemd/system" \
    "${OUTPUT_DIR}"

install -m 0755 \
    "${BINARY_PATH}" \
    "${STAGE_DIR}/usr/local/bin/edge_gateway"

install -m 0644 \
    "${PROJECT_DIR}/VERSION" \
    "${STAGE_DIR}/usr/local/share/linux-iot-edge-gateway/VERSION"

install -m 0640 \
    "${PROJECT_DIR}/config/gateway.systemd.yaml" \
    "${STAGE_DIR}/etc/linux-iot-edge-gateway/gateway.yaml"

install -m 0644 \
    "${PROJECT_DIR}/deploy/systemd/linux-iot-edge-gateway.service" \
    "${STAGE_DIR}/lib/systemd/system/linux-iot-edge-gateway.service"

tar \
    -C "${STAGE_DIR}" \
    -czf "${ARTIFACT_PATH}" \
    .

(
    cd "${OUTPUT_DIR}"
    sha256sum "${ARTIFACT_NAME}" \
        > "$(basename "${CHECKSUM_PATH}")"
)

echo "[INFO] ARM64 dynamic dependencies"
readelf -d "${BINARY_PATH}" \
    | grep 'Shared library' || true

echo "[PASS] ARM64 ELF: $(file "${BINARY_PATH}")"
echo "[PASS] deployment artifact: ${ARTIFACT_PATH}"
echo "[PASS] SHA-256 checksum: ${CHECKSUM_PATH}"
