#!/usr/bin/env python3

import sys
import time
import serial


GOOD_FRAME = bytes([
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
    0x4D, 0x58
])


def main():
    if len(sys.argv) < 2:
        print(
            "Usage: python3 scripts/inject_stream_cases.py "
            "<serial_device>"
        )
        return 1

    device = sys.argv[1]
    split_position = 6

    first_part = GOOD_FRAME[:split_position]
    second_part = GOOD_FRAME[split_position:]

    with serial.Serial(
        device,
        baudrate=115200,
        timeout=1
    ) as serial_port:

        serial_port.write(first_part)
        serial_port.flush()

        print(
            "TX half part 1: "
            + first_part.hex(" ").upper()
        )

        time.sleep(0.5)

        serial_port.write(second_part)
        serial_port.flush()

        print(
            "TX half part 2: "
            + second_part.hex(" ").upper()
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
