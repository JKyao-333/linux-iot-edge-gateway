#!/usr/bin/env python3

import argparse
from datetime import datetime, timezone
import getpass
import ipaddress
import json
import os
from pathlib import Path
import re
import sys


PROJECT_DIR = Path(__file__).resolve().parent.parent
IPV4_PATTERN = re.compile(r"(?<![\d.])(?:\d{1,3}\.){3}\d{1,3}(?![\d.])")
IPV6_PATTERN = re.compile(
    r"(?<![0-9A-Fa-f:])(?:[0-9A-Fa-f]{0,4}:){2,7}"
    r"[0-9A-Fa-f]{0,4}(?![0-9A-Fa-f:])"
)
SERIAL_PATTERN = re.compile(r"/dev/serial/by-id/[^\s,;]+")
POSIX_HOME_PATTERN = re.compile(r"/home/[^/\s]+")
WSL_HOME_PATTERN = re.compile(r"/mnt/[A-Za-z]/Users/[^/\s]+", re.IGNORECASE)
WINDOWS_HOME_PATTERN = re.compile(
    r"[A-Za-z]:[\\/]Users[\\/][^\\/\s]+",
    re.IGNORECASE,
)
SECRET_KEY_PATTERN = re.compile(
    r"^(?:password|passwd|token|access_token|api_key|private_key|"
    r"private_key_file|certificate|certificate_file|cert_file|username|"
    r"mqtt_username|mqtt_password)$",
    re.IGNORECASE,
)
SECRET_ASSIGNMENT_PATTERN = re.compile(
    r"(?i)([\"']?(?:password|passwd|token|access_token|api_key|"
    r"private_key|private_key_file|certificate|certificate_file|cert_file|"
    r"username|mqtt_username|mqtt_password)[\"']?\s*[:=]\s*)"
    r"(\"[^\"]*\"|'[^']*'|[^\s,;]+)"
)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Sanitize gateway logs and validation reports."
    )
    parser.add_argument("--input", required=True)
    parser.add_argument(
        "--output",
        help="Output file path. Required unless --dry-run is used.",
    )
    parser.add_argument(
        "--redact-ip", action=argparse.BooleanOptionalAction, default=True
    )
    parser.add_argument(
        "--redact-path", action=argparse.BooleanOptionalAction, default=True
    )
    parser.add_argument(
        "--redact-user", action=argparse.BooleanOptionalAction, default=True
    )
    parser.add_argument(
        "--redact-serial", action=argparse.BooleanOptionalAction, default=True
    )
    parser.add_argument(
        "--redact-secrets", action=argparse.BooleanOptionalAction, default=True
    )
    parser.add_argument("--append-note", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    return parser.parse_args()


def redact_ipv4(match):
    value = match.group(0)
    try:
        address = ipaddress.ip_address(value)
    except ValueError:
        return value
    if not isinstance(address, ipaddress.IPv4Address):
        return value
    octets = value.split(".")
    if address.is_private or address.is_loopback or address.is_link_local:
        return f"{octets[0]}.{octets[1]}.x.x"
    return "<redacted-ipv4>"


def redact_ipv6(match):
    value = match.group(0)
    try:
        address = ipaddress.ip_address(value)
    except ValueError:
        return value
    return "<redacted-ipv6>" if isinstance(address, ipaddress.IPv6Address) else value


def redact_assignment(match):
    prefix = match.group(1)
    value = match.group(2)
    if value.startswith('"') and value.endswith('"'):
        replacement = '"<redacted>"'
    elif value.startswith("'") and value.endswith("'"):
        replacement = "'<redacted>'"
    else:
        replacement = "<redacted>"
    return prefix + replacement


class Sanitizer:
    def __init__(self, args):
        self.args = args
        self.repo_variants = {
            str(PROJECT_DIR),
            str(PROJECT_DIR).replace("\\", "/"),
        }
        self.home_variants = {
            str(Path.home()),
            str(Path.home()).replace("\\", "/"),
        }
        self.user_names = {
            name
            for name in {
                os.environ.get("USER"),
                os.environ.get("USERNAME"),
                getpass.getuser(),
            }
            if name
        }

    def sanitize_text(self, value):
        text = value
        if self.args.redact_path:
            for repo_path in sorted(self.repo_variants, key=len, reverse=True):
                text = text.replace(repo_path, "<repo-root>")
            for home_path in sorted(self.home_variants, key=len, reverse=True):
                text = text.replace(home_path, "<home>")
            text = WSL_HOME_PATTERN.sub("<windows-home>", text)
            text = WINDOWS_HOME_PATTERN.sub("<windows-home>", text)
            text = POSIX_HOME_PATTERN.sub("<home>", text)
        if self.args.redact_serial:
            text = SERIAL_PATTERN.sub(
                "/dev/serial/by-id/<redacted-serial>", text
            )
        if self.args.redact_user:
            for user_name in sorted(self.user_names, key=len, reverse=True):
                text = re.sub(
                    rf"(?<![\w.-]){re.escape(user_name)}(?![\w.-])",
                    "<redacted-user>",
                    text,
                    flags=re.IGNORECASE,
                )
        if self.args.redact_ip:
            text = IPV4_PATTERN.sub(redact_ipv4, text)
            text = IPV6_PATTERN.sub(redact_ipv6, text)
        if self.args.redact_secrets:
            text = SECRET_ASSIGNMENT_PATTERN.sub(redact_assignment, text)
        return text

    def sanitize_json(self, value, key=""):
        if self.args.redact_secrets and SECRET_KEY_PATTERN.match(str(key)):
            return "<redacted>"
        if isinstance(value, dict):
            return {
                item_key: self.sanitize_json(item_value, str(item_key))
                for item_key, item_value in value.items()
            }
        if isinstance(value, list):
            return [self.sanitize_json(item, key) for item in value]
        if isinstance(value, str):
            return self.sanitize_text(value)
        return value

    def sanitize_line(self, line):
        stripped = line.strip()
        if stripped:
            try:
                parsed = json.loads(stripped)
            except json.JSONDecodeError:
                pass
            else:
                sanitized = self.sanitize_json(parsed)
                return json.dumps(sanitized, ensure_ascii=False, separators=(",", ":"))
        return self.sanitize_text(line)


def main():
    args = parse_args()

    if not args.dry_run and not args.output:
        print(
            "[ERROR] --output is required unless --dry-run is used",
            file=sys.stderr,
        )
        return 2
    if args.dry_run and args.output:
        print("[WARN] --output is ignored in dry-run mode", file=sys.stderr)

    input_path = Path(args.input).expanduser()

    if not input_path.exists() or not input_path.is_file():
        print(
            f"[ERROR] input file not found: {input_path.name or '<input>'}",
            file=sys.stderr,
        )
        return 1

    try:
        source = input_path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        print(f"[ERROR] cannot read input: {error.strerror}", file=sys.stderr)
        return 1

    sanitizer = Sanitizer(args)
    had_final_newline = source.endswith("\n")
    sanitized_lines = [sanitizer.sanitize_line(line) for line in source.splitlines()]
    result = "\n".join(sanitized_lines)
    if had_final_newline and result:
        result += "\n"

    if args.append_note:
        note_time = datetime.now(timezone.utc).astimezone().isoformat(
            timespec="seconds"
        )
        if result and not result.endswith("\n"):
            result += "\n"
        result += f"# Sanitized by scripts/sanitize_logs.py at {note_time}\n"

    if args.dry_run:
        sys.stdout.write(result)
        return 0

    output_path = Path(args.output).expanduser()
    try:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(result, encoding="utf-8")
    except OSError as error:
        print(f"[ERROR] cannot write output: {error.strerror}", file=sys.stderr)
        return 1

    print(f"[PASS] sanitized log written: {output_path.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
