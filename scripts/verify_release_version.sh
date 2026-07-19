#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

VERSION_FILE="${PROJECT_DIR}/VERSION"

if [[ ! -f "${VERSION_FILE}" ]]; then
    echo "[ERROR] version file not found: ${VERSION_FILE}"
    exit 1
fi

VERSION="$(tr -d '\r\n[:space:]' < "${VERSION_FILE}")"

if [[ ! "${VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "[ERROR] VERSION must use MAJOR.MINOR.PATCH: ${VERSION}"
    exit 1
fi

if [[ $# -ge 1 ]]; then
    EXPECTED_TAG="v${VERSION}"

    if [[ "$1" != "${EXPECTED_TAG}" ]]; then
        echo "[ERROR] release tag $1 does not match VERSION ${VERSION}"
        echo "[INFO] expected tag: ${EXPECTED_TAG}"
        exit 1
    fi
fi

echo "${VERSION}"
