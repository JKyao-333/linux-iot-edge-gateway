#!/usr/bin/env bash

set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    echo "[ERROR] Run this script with sudo"
    exit 1
fi

SERVICE_NAME="linux-iot-edge-gateway"
SERVICE_USER="iot-gateway"
PURGE_DATA=false

if [[ "${1:-}" == "--purge" ]]; then
    PURGE_DATA=true
elif [[ -n "${1:-}" ]]; then
    echo "Usage: sudo $0 [--purge]"
    exit 1
fi

systemctl disable --now "${SERVICE_NAME}.service" \
    2>/dev/null || true

rm -f -- \
    "/etc/systemd/system/${SERVICE_NAME}.service" \
    /usr/local/bin/edge_gateway

systemctl daemon-reload

if [[ "${PURGE_DATA}" == true ]]; then
    rm -rf -- \
        "/etc/${SERVICE_NAME}" \
        "/var/lib/${SERVICE_NAME}" \
        "/var/log/${SERVICE_NAME}"

    if id "${SERVICE_USER}" >/dev/null 2>&1; then
        userdel "${SERVICE_USER}"
    fi

    echo "[PASS] service, configuration and runtime data removed"
else
    echo "[PASS] service removed; configuration and runtime data preserved"
    echo "[INFO] use --purge to remove preserved data"
fi
