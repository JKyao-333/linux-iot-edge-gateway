#!/usr/bin/env bash

set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    echo "[ERROR] Run this script with sudo"
    exit 1
fi

PROJECT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

SERVICE_NAME="linux-iot-edge-gateway"
SERVICE_USER="iot-gateway"
SERIAL_DEVICE="${1:-/dev/ttyUSB0}"

if [[ "${SERIAL_DEVICE}" != /dev/* ]]; then
    echo "[ERROR] Serial device must be an absolute /dev path"
    exit 1
fi

INVOKING_USER="${SUDO_USER:-root}"
INVOKING_HOME="$(
    getent passwd "${INVOKING_USER}" | cut -d: -f6
)"

GATEWAY_BIN="${GATEWAY_BIN:-${INVOKING_HOME}/linux-iot-edge-gateway-build/edge_gateway}"
CONFIG_TEMPLATE="${PROJECT_DIR}/config/gateway.systemd.yaml"
SERVICE_TEMPLATE="${PROJECT_DIR}/deploy/systemd/${SERVICE_NAME}.service"

for required_file in \
    "${GATEWAY_BIN}" \
    "${CONFIG_TEMPLATE}" \
    "${SERVICE_TEMPLATE}"
do
    if [[ ! -f "${required_file}" ]]; then
        echo "[ERROR] Required file not found: ${required_file}"
        exit 1
    fi
done

if ! id "${SERVICE_USER}" >/dev/null 2>&1; then
    useradd \
        --system \
        --user-group \
        --home-dir /var/lib/${SERVICE_NAME} \
        --create-home \
        --shell /usr/sbin/nologin \
        "${SERVICE_USER}"
fi

usermod -aG dialout "${SERVICE_USER}"

install -d -m 0750 \
    -o "${SERVICE_USER}" \
    -g "${SERVICE_USER}" \
    "/var/lib/${SERVICE_NAME}" \
    "/var/log/${SERVICE_NAME}"

install -d -m 0750 \
    -o root \
    -g "${SERVICE_USER}" \
    "/etc/${SERVICE_NAME}"

install -m 0755 \
    "${GATEWAY_BIN}" \
    /usr/local/bin/edge_gateway

TEMP_CONFIG="$(mktemp)"
trap 'rm -f -- "${TEMP_CONFIG}"' EXIT

sed \
    "s|^  device:.*$|  device: ${SERIAL_DEVICE}|" \
    "${CONFIG_TEMPLATE}" \
    >"${TEMP_CONFIG}"

install -m 0640 \
    -o root \
    -g "${SERVICE_USER}" \
    "${TEMP_CONFIG}" \
    "/etc/${SERVICE_NAME}/gateway.yaml"

install -m 0644 \
    "${SERVICE_TEMPLATE}" \
    "/etc/systemd/system/${SERVICE_NAME}.service"

systemctl daemon-reload
systemctl enable "${SERVICE_NAME}.service"
systemctl restart "${SERVICE_NAME}.service"

echo "[PASS] ${SERVICE_NAME} installed"
echo "[INFO] serial device: ${SERIAL_DEVICE}"
echo "[INFO] status: systemctl status ${SERVICE_NAME}"
echo "[INFO] logs: journalctl -u ${SERVICE_NAME} -f"
