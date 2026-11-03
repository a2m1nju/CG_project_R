"""Build the server, then run: python ItemTestServer/smoke_test.py"""

from pathlib import Path
import socket
import subprocess
import time


SERVER = Path(__file__).resolve().parent / "x64" / "Release" / "ItemTestServer.exe"


def read_line(connection):
    data = bytearray()
    while not data.endswith(b"\n"):
        chunk = connection.recv(1)
        if not chunk:
            raise AssertionError("server disconnected")
        data.extend(chunk)
    return data.decode("ascii").strip()


def send_bytes(connection, data):
    remaining = memoryview(data)
    while remaining:
        count = connection.send(remaining)
        if count == 0:
            raise AssertionError("server disconnected during send")
        remaining = remaining[count:]


def command(connection, request, expected):
    send_bytes(connection, (request + "\n").encode("ascii"))
    actual = read_line(connection)
    assert actual == expected, f"{request!r}: {actual!r} != {expected!r}"


def main():
    if not SERVER.exists():
        raise SystemExit(f"Build Release|x64 first: {SERVER}")
    process = subprocess.Popen([str(SERVER)], stdout=subprocess.DEVNULL)
    try:
        deadline = time.monotonic() + 5
        while True:
            try:
                connection = socket.create_connection(("127.0.0.1", 19001), timeout=1)
                break
            except OSError:
                if process.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("test server did not start")
                time.sleep(0.05)

        with connection:
            connection.settimeout(2)
            assert read_line(connection) == "OK item test server ready"
            # A command split across TCP sends must still be read as one line.
            send_bytes(connection, b"ADD_")
            send_bytes(connection, b"PLAYER 1\n")
            assert read_line(connection) == "OK player"
            command(connection, "ADD_PLAYER 2", "OK player")
            command(connection, "SPAWN 10 1 0 0", "OK item")
            command(connection, "SPAWN 10 2 1 0", "ERR invalid_or_duplicate_item")
            command(connection, "TICK 2 2 0 0 1 1 0 0 1", "OK pickups=1 player=1:item=10:slot=0")
            command(connection, "STATUS 1", "OK slots=1,0 selected=0 ground=0")
            command(connection, "SPAWN 11 2 0 0", "OK item")
            command(connection, "PICKUP 1 0 0", "OK pickups=1 item=11:slot=1")
            command(connection, "SPAWN 12 3 0 0", "OK item")
            command(connection, "PICKUP 1 0 0", "OK pickups=0")
            command(connection, "STATUS 1", "OK slots=1,2 selected=0 ground=1")
            command(connection, "PICKUP 2 0 0", "OK pickups=1 item=12:slot=0")
            command(connection, "RESET", "OK reset")
            command(connection, "STATUS 1", "ERR unknown_player")
            command(connection, "QUIT", "BYE")
        print("PASS: TCP framing, deterministic pickup, capacity, reset")
    finally:
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


if __name__ == "__main__":
    main()
