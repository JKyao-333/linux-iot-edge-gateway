#!/usr/bin/env python3

import sys
import serial


BAD_CRC_FRAME = bytes([
    0xAA, 0x55,
    0x0B,
    0x01,
    0x10,
    0x00, 0xFD,
    0x02, 0x60,
    0x01, 0x2C,
    0x0E, 0x74,
    0x00,
    0x00, 0x01,
    0x00, 0x00
])


BAD_LENGTH_FRAME = bytes([
    0xAA, 0x55,
    0xFF,
    0x01,
    0x10,
    0x00, 0x00
])


def print_usage():
    print(
        "Usage: python3 scripts/inject_bad_frames.py "
        "<serial_device> <crc|length>"
    )


def main():
    if len(sys.argv) < 3:
        print_usage()
        return 1

    device = sys.argv[1]
    error_type = sys.argv[2]

    if error_type == "crc":
        frame = BAD_CRC_FRAME
    elif error_type == "length":
        frame = BAD_LENGTH_FRAME
    else:
        print(f"Unknown error type: {error_type}")
        print_usage()
        return 1

    with serial.Serial(
        device,
        baudrate=115200,
        timeout=1
    ) as serial_port:

        serial_port.write(frame)
        serial_port.flush()

    print(
        f"TX {error_type} error frame: "
        + frame.hex(" ").upper()
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
