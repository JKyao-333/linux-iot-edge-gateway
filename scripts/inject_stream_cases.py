#!/usr/bin/env python3

import sys
import time
import serial


PAYLOAD = bytes([
    0x00, 0xFD,
    0x02, 0x60,
    0x01, 0x2C,
    0x0E, 0x74,
    0x00,
    0x00, 0x01,
])


def crc16_modbus(data):
    crc = 0xFFFF

    for byte in data:
        crc ^= byte

        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1

    return crc


def build_frame(device_id):
    body = bytes([
        0x0B,
        0x01,
        device_id,
    ]) + PAYLOAD

    crc = crc16_modbus(body)

    return bytes([0xAA, 0x55]) + body + bytes([
        crc & 0xFF,
        (crc >> 8) & 0xFF,
    ])


def main():
    if len(sys.argv) < 2:
        print(
            "Usage: python3 scripts/inject_stream_cases.py "
            "<serial_device> [device_id]"
        )
        return 1

    device = sys.argv[1]
    device_id = int(sys.argv[2], 0) if len(sys.argv) >= 3 else 0x10

    if not 0 <= device_id <= 0xFF:
        print("device_id must be between 0 and 255")
        return 1

    good_frame = build_frame(device_id)
    split_position = 6

    first_part = good_frame[:split_position]
    second_part = good_frame[split_position:]

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
