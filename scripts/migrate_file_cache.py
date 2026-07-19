#!/usr/bin/env python3

import argparse
import sqlite3
from pathlib import Path


CREATE_TABLE_SQL = """
CREATE TABLE IF NOT EXISTS pending_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    topic TEXT NOT NULL,
    payload TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
)
"""


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Migrate the legacy text cache to SQLite."
    )
    parser.add_argument("source", type=Path)
    parser.add_argument("target", type=Path)
    return parser.parse_args()


def load_legacy_messages(path: Path) -> list[tuple[str, str]]:
    messages: list[tuple[str, str]] = []

    with path.open("r", encoding="utf-8") as source:
        for line_number, raw_line in enumerate(source, start=1):
            line = raw_line.rstrip("\r\n")

            if not line:
                continue

            topic, separator, payload = line.partition("\t")

            if not separator or not topic or not payload:
                raise ValueError(
                    f"malformed cache line: {line_number}"
                )

            messages.append((topic, payload))

    return messages


def migrate(source_path: Path, target_path: Path) -> int:
    if not source_path.is_file():
        raise FileNotFoundError(
            f"legacy cache not found: {source_path}"
        )

    backup_path = source_path.with_name(
        source_path.name + ".migrated"
    )

    if backup_path.exists():
        raise FileExistsError(
            f"migration backup already exists: {backup_path}"
        )

    messages = load_legacy_messages(source_path)
    target_path.parent.mkdir(parents=True, exist_ok=True)

    with sqlite3.connect(target_path) as database:
        database.execute(CREATE_TABLE_SQL)

        existing_count = database.execute(
            "SELECT COUNT(*) FROM pending_messages"
        ).fetchone()[0]

        if existing_count != 0:
            raise RuntimeError(
                "target SQLite cache is not empty"
            )

        database.executemany(
            "INSERT INTO pending_messages(topic, payload) "
            "VALUES(?, ?)",
            messages,
        )

    source_path.rename(backup_path)
    return len(messages)


def main() -> int:
    arguments = parse_arguments()

    try:
        migrated_count = migrate(
            arguments.source,
            arguments.target,
        )
    except (OSError, ValueError, RuntimeError, sqlite3.Error) as error:
        print(f"[ERROR] {error}")
        return 1

    print(f"[PASS] migrated messages: {migrated_count}")
    print(
        "[INFO] legacy backup: "
        f"{arguments.source}.migrated"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
