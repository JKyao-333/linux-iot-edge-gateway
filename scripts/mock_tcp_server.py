#!/usr/bin/env python3

import socket
import os
import sys


def handle_client(connection, address):
    print(f"[INFO] client connected: {address[0]}:{address[1]}")

    buffer = b""

    while True:
        chunk = connection.recv(1024)

        if not chunk:
            break

        buffer += chunk

        while b"\n" in buffer:
            one_line, buffer = buffer.split(b"\n", 1)

            if not one_line:
                continue

            try:
                message = one_line.decode("utf-8")
            except UnicodeDecodeError:
                print("[WARN] received invalid UTF-8 data")
                continue

            print(f"[RX] {message}")

    if buffer:
        print(
            "[WARN] client disconnected with "
            f"{len(buffer)} incomplete bytes"
        )

    print(f"[INFO] client disconnected: {address[0]}:{address[1]}")


def main():
    host = os.environ.get("TCP_SERVER_HOST", "127.0.0.1")
    port = 9000

    if len(sys.argv) >= 2:
        port = int(sys.argv[1])

    with socket.socket(
        socket.AF_INET,
        socket.SOCK_STREAM
    ) as server_socket:
        server_socket.setsockopt(
            socket.SOL_SOCKET,
            socket.SO_REUSEADDR,
            1
        )

        server_socket.bind((host, port))
        server_socket.listen(5)

        print(f"[INFO] TCP server listening on {host}:{port}")

        while True:
            connection, address = server_socket.accept()

            with connection:
                handle_client(connection, address)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n[INFO] TCP server stopped")
