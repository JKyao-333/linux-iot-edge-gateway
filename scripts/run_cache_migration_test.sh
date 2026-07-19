#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

RUNTIME_DIR="$(mktemp -d /tmp/iot-cache-migration.XXXXXX)"
LEGACY_CACHE="${RUNTIME_DIR}/pending_messages.cache"
SQLITE_CACHE="${RUNTIME_DIR}/pending_messages.db"

cleanup() {
    rm -rf -- "${RUNTIME_DIR}"
}

trap cleanup EXIT INT TERM

printf '%s\t%s\n' \
    'sensor/16/data' \
    '{"sequence":101,"valid":true}' \
    >"${LEGACY_CACHE}"

printf '%s\t%s\n' \
    'sensor/17/data' \
    '{"sequence":102,"valid":true}' \
    >>"${LEGACY_CACHE}"

python3 \
    "${PROJECT_DIR}/scripts/migrate_file_cache.py" \
    "${LEGACY_CACHE}" \
    "${SQLITE_CACHE}"

MIGRATED_COUNT="$(
    sqlite3 "${SQLITE_CACHE}" \
        'SELECT COUNT(*) FROM pending_messages;'
)"

FIRST_SEQUENCE="$(
    sqlite3 "${SQLITE_CACHE}" \
        "SELECT json_extract(payload, '$.sequence') FROM pending_messages ORDER BY id LIMIT 1;"
)"

if [[ "${MIGRATED_COUNT}" != "2" \
      || "${FIRST_SEQUENCE}" != "101" \
      || ! -f "${LEGACY_CACHE}.migrated" \
      || -e "${LEGACY_CACHE}" ]]; then

    echo "[ERROR] cache migration verification failed"
    exit 1
fi

echo "[PASS] file-to-SQLite cache migration test passed"
