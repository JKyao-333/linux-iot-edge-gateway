#!/usr/bin/env python3

import sys
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
            "Usage: python3 scripts/inject_sticky_frames.py "
            "<serial_device>"
        )
        return 1

    device = sys.argv[1]

    # 两个完整帧连续排列，模拟串口粘包。
    sticky_data = GOOD_FRAME + GOOD_FRAME

    with serial.Serial(
        device,
        baudrate=115200,
        timeout=1
    ) as serial_port:
        serial_port.write(sticky_data)
        serial_port.flush()

    print(
        f"TX sticky data ({len(sticky_data)} bytes): "
        + sticky_data.hex(" ").upper()
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
