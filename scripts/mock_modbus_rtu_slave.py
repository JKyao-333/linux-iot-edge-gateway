#!/usr/bin/env python3

import argparse
import signal
import sys

import serial


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc & 0xFFFF


def append_crc(payload: bytes) -> bytes:
    crc = crc16_modbus(payload)
    return payload + bytes((crc & 0xFF, crc >> 8))


def main() -> int:
    parser = argparse.ArgumentParser(description="Minimal Modbus RTU test slave")
    parser.add_argument("device")
    parser.add_argument("--baud-rate", type=int, default=115200)
    parser.add_argument("--slave-id", type=int, default=1)
    args = parser.parse_args()

    running = True

    def stop(_signum, _frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)

    registers = (253, 608, 300, 3700, 0, 1)
    with serial.Serial(args.device, args.baud_rate, timeout=0.2) as port:
        while running:
            request = port.read(8)
            if not request:
                continue
            if len(request) != 8 or crc16_modbus(request[:-2]) != int.from_bytes(
                request[-2:], "little"
            ):
                print("[WARN] invalid Modbus request", flush=True)
                continue

            slave_id, function = request[0], request[1]
            count = int.from_bytes(request[4:6], "big")
            if slave_id != args.slave_id:
                continue
            if function not in (3, 4):
                port.write(append_crc(bytes((slave_id, function | 0x80, 1))))
                continue
            if count != len(registers):
                port.write(append_crc(bytes((slave_id, function | 0x80, 3))))
                continue

            payload = b"".join(value.to_bytes(2, "big") for value in registers)
            response = append_crc(bytes((slave_id, function, len(payload))) + payload)
            port.write(response)
            port.flush()
            print("[TX] Modbus response", flush=True)

    return 0


if __name__ == "__main__":
    sys.exit(main())
