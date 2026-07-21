#!/usr/bin/env python3

import argparse
from datetime import datetime, timezone
from pathlib import Path
import re
import sys
import time


PROJECT_DIR = Path(__file__).resolve().parent.parent
DEFAULT_SUMMARY = PROJECT_DIR / "artifacts" / "serial_replay_summary.md"
HEX_BYTE = re.compile(r"^[0-9A-Fa-f]{2}$")
MAX_SUMMARY_ROWS = 1000


class ReplayError(Exception):
    pass


def timestamp():
    return datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Replay hexadecimal frames to a serial device."
    )
    parser.add_argument("--input", required=True, help=".hex or .txt frame file")
    parser.add_argument("--serial", required=True, help="serial device")
    parser.add_argument("--baudrate", type=int, default=115200)
    parser.add_argument("--rate-hz", type=float, default=1.0)
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--split-bytes", type=int, default=0)
    parser.add_argument("--split-delay-ms", type=float, default=50.0)
    parser.add_argument("--append-newline", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--summary", default=str(DEFAULT_SUMMARY))
    args = parser.parse_args()

    if args.baudrate <= 0:
        parser.error("--baudrate must be positive")
    if args.rate_hz <= 0:
        parser.error("--rate-hz must be greater than zero")
    if args.repeat < 0:
        parser.error("--repeat must be zero or a positive integer")
    if args.split_bytes < 0:
        parser.error("--split-bytes must be zero or a positive integer")
    if args.split_delay_ms < 0:
        parser.error("--split-delay-ms must not be negative")

    return args


def load_frames(path):
    if not path.exists():
        raise ReplayError(f"input file not found: {path.name or '<input>'}")
    if not path.is_file():
        raise ReplayError(f"input path is not a file: {path.name or '<input>'}")
    if path.suffix.lower() not in {".hex", ".txt"}:
        raise ReplayError("input file must use .hex or .txt extension")

    frames = []
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError) as error:
        raise ReplayError(f"cannot read input file: {error}") from error

    for line_number, raw_line in enumerate(lines, start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue

        tokens = line.split()
        invalid = next((token for token in tokens if not HEX_BYTE.fullmatch(token)), None)
        if invalid is not None:
            raise ReplayError(
                f"invalid hexadecimal byte at line {line_number}: {invalid!r}"
            )
        frames.append((line_number, bytes(int(token, 16) for token in tokens)))

    if not frames:
        raise ReplayError("input file contains no replay frames")
    return frames


def serial_label(device):
    normalized = device.replace("\\", "/")
    if "/dev/serial/by-id/" in normalized:
        return "/dev/serial/by-id/<redacted-serial>"
    name = Path(normalized).name
    return name if name else "<serial-device>"


def write_frame(serial_port, payload, split_bytes, split_delay_seconds):
    if split_bytes <= 0:
        chunks = [payload]
    else:
        chunks = [
            payload[index:index + split_bytes]
            for index in range(0, len(payload), split_bytes)
        ]

    for index, chunk in enumerate(chunks):
        written = serial_port.write(chunk)
        if written != len(chunk):
            raise ReplayError(
                f"serial write interrupted: expected {len(chunk)} bytes, wrote {written}"
            )
        serial_port.flush()
        if index + 1 < len(chunks) and split_delay_seconds > 0:
            time.sleep(split_delay_seconds)


def write_summary(path, args, started_at, ended_at, result, error_message,
                  sent_frames, sent_bytes, rows, omitted_rows):
    repeat_text = "infinite" if args.repeat == 0 else str(args.repeat)
    lines = [
        "# Serial Replay Summary",
        "",
        f"- Start: {started_at}",
        f"- End: {ended_at}",
        f"- Result: **{result}**",
        f"- Input: `{Path(args.input).name}`",
        f"- Serial: `{serial_label(args.serial)}`",
        f"- Baud rate: `{args.baudrate}`",
        f"- Rate: `{args.rate_hz:g} Hz`",
        f"- Repeat: `{repeat_text}`",
        f"- Split bytes: `{args.split_bytes}`",
        f"- Dry run: `{str(args.dry_run).lower()}`",
        f"- Total sent frames: `{sent_frames}`",
        f"- Total sent bytes: `{sent_bytes}`",
        "",
    ]
    if error_message:
        lines.extend(["## Error", "", error_message, ""])
    lines.extend([
        "## Frame Results",
        "",
        "| Number | Source line | Bytes | Status | Hex |",
        "| ---: | ---: | ---: | --- | --- |",
    ])
    for row in rows:
        lines.append(
            f"| {row['number']} | {row['line']} | {row['bytes']} | "
            f"{row['status']} | `{row['hex']}` |"
        )
    if omitted_rows:
        lines.append(f"| - | - | - | INFO | `{omitted_rows} additional rows omitted` |")
    lines.extend([
        "",
        "## Boundary",
        "",
        "Replay reproduces protocol and transport input only. It does not replace "
        "long-duration hardware validation or represent field electrical, EMC, or "
        "power-disturbance testing.",
    ])

    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    except OSError as error:
        raise ReplayError(f"cannot write summary: {error}") from error


def main():
    args = parse_args()
    started_at = timestamp()
    summary_path = Path(args.summary).expanduser()
    rows = []
    omitted_rows = 0
    sent_frames = 0
    sent_bytes = 0
    serial_port = None
    result = "FAIL"
    error_message = ""
    exit_code = 1

    try:
        frames = load_frames(Path(args.input).expanduser())
        if not args.dry_run:
            try:
                import serial
            except ImportError as error:
                raise ReplayError(
                    "pyserial is required; install python3-serial or run "
                    "'pip install pyserial'"
                ) from error
            try:
                serial_port = serial.Serial(
                    args.serial,
                    baudrate=args.baudrate,
                    timeout=1,
                    write_timeout=2,
                )
            except serial.SerialException as error:
                raise ReplayError(
                    f"cannot open serial device: {serial_label(args.serial)} "
                    f"({type(error).__name__})"
                ) from error

        sequence = 0
        iteration = 0
        while args.repeat == 0 or iteration < args.repeat:
            iteration += 1
            for source_line, frame in frames:
                sequence += 1
                payload = frame + (b"\n" if args.append_newline else b"")
                frame_hex = " ".join(f"{byte:02X}" for byte in payload)
                status = "DRY-RUN" if args.dry_run else "SENT"

                if not args.dry_run:
                    try:
                        write_frame(
                            serial_port,
                            payload,
                            args.split_bytes,
                            args.split_delay_ms / 1000.0,
                        )
                    except ReplayError as error:
                        raise ReplayError(f"frame {sequence} write failed: {error}") from error
                    except (OSError, serial.SerialException) as error:
                        raise ReplayError(
                            f"frame {sequence} write failed ({type(error).__name__})"
                        ) from error
                    sent_frames += 1
                    sent_bytes += len(payload)

                print(
                    f"frame={sequence} bytes={len(payload)} status={status} hex={frame_hex}",
                    flush=True,
                )
                row = {
                    "number": sequence,
                    "line": source_line,
                    "bytes": len(payload),
                    "status": status,
                    "hex": frame_hex,
                }
                if len(rows) < MAX_SUMMARY_ROWS:
                    rows.append(row)
                else:
                    omitted_rows += 1

                if not args.dry_run:
                    time.sleep(1.0 / args.rate_hz)

        result = "PASS"
        exit_code = 0
    except KeyboardInterrupt:
        result = "INTERRUPTED"
        error_message = "Replay interrupted by the operator."
        exit_code = 130
    except ReplayError as error:
        error_message = str(error)
        print(f"[ERROR] {error_message}", file=sys.stderr)
    finally:
        if serial_port is not None:
            serial_port.close()

    try:
        write_summary(
            summary_path,
            args,
            started_at,
            timestamp(),
            result,
            error_message,
            sent_frames,
            sent_bytes,
            rows,
            omitted_rows,
        )
        print(f"summary={serial_label(str(summary_path))}")
    except ReplayError as error:
        print(f"[ERROR] {error}", file=sys.stderr)
        return 1
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
