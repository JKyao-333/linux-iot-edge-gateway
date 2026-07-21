#!/usr/bin/env python3

import argparse
import time
import serial


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Continuously send a valid sensor frame."
    )
    parser.add_argument("serial_device")
    parser.add_argument("--rate-hz", type=float, default=1.0)
    parser.add_argument(
        "--count",
        type=int,
        default=0,
        help="Stop after this many frames; 0 means run continuously.",
    )
    return parser.parse_args()


def main():
    arguments = parse_arguments()

    if arguments.rate_hz <= 0:
        print("--rate-hz must be greater than zero")
        return 1

    if arguments.count < 0:
        print("--count must not be negative")
        return 1

    device = arguments.serial_device
    interval_seconds = 1.0 / arguments.rate_hz
    frame = bytes([
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
    with serial.Serial(device, baudrate=115200, timeout=1) as ser:
        count = 0

        while arguments.count == 0 or count < arguments.count:
            ser.write(frame)
            ser.flush()

            count += 1
            print(
                f"TX frame #{count}: {frame.hex(' ').upper()}",
                flush=True,
            )

            time.sleep(interval_seconds)

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\n[INFO] serial sender stopped")
