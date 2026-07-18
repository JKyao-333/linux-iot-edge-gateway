#!/usr/bin/env python3

import sys
import time
import serial


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 scripts/mock_serial_sender.py <serial_device>")
        print("Example: python3 scripts/mock_serial_sender.py /tmp/tty_stm32")
        return 1

    device = sys.argv[1]
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

        while True:
            ser.write(frame)
            ser.flush()

            count += 1
            print(f"TX frame #{count}: {frame.hex(' ').upper()}")

            time.sleep(1)


if __name__ == "__main__":
    raise SystemExit(main())
