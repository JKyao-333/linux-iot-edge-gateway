#!/usr/bin/env python3

import argparse
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


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Inject valid serial stream boundary cases."
    )
    parser.add_argument("serial_device")
    parser.add_argument("device_id", nargs="?", default="0x10")
    parser.add_argument(
        "--mode",
        choices=("normal", "half", "noise"),
        default="half",
    )
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    device = arguments.serial_device
    device_id = int(arguments.device_id, 0)

    if not 0 <= device_id <= 0xFF:
        print("device_id must be between 0 and 255")
        return 1

    good_frame = build_frame(device_id)
    with serial.Serial(
        device,
        baudrate=115200,
        timeout=1
    ) as serial_port:
        if arguments.mode == "normal":
            serial_port.write(good_frame)
            serial_port.flush()
            print("TX normal frame: " + good_frame.hex(" ").upper())
        elif arguments.mode == "noise":
            noise = bytes([0x00, 0x13, 0xAA, 0x7E, 0x55])
            serial_port.write(noise + good_frame)
            serial_port.flush()
            print(
                "TX noise plus frame: "
                + (noise + good_frame).hex(" ").upper()
            )
        else:
            split_position = 6
            first_part = good_frame[:split_position]
            second_part = good_frame[split_position:]

            serial_port.write(first_part)
            serial_port.flush()
            print("TX half part 1: " + first_part.hex(" ").upper())
            time.sleep(0.5)
            serial_port.write(second_part)
            serial_port.flush()
            print("TX half part 2: " + second_part.hex(" ").upper())

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
