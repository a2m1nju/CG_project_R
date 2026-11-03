"""Interactive client for the local item test server."""

import socket


def send_command(connection, command):
    remaining = memoryview((command + "\n").encode("ascii"))
    while remaining:
        count = connection.send(remaining)
        if count == 0:
            raise ConnectionError("Server disconnected during send")
        remaining = remaining[count:]


def main():
    with socket.create_connection(("127.0.0.1", 19001), timeout=5) as connection:
        with connection.makefile("r", encoding="ascii", newline="\n") as incoming:
            print(incoming.readline().strip())
            print("Enter a command, or QUIT to disconnect.")
            while True:
                try:
                    command = input("> ")
                except EOFError:
                    command = "QUIT"
                send_command(connection, command)
                response = incoming.readline()
                if not response:
                    print("Server disconnected.")
                    break
                print(response.strip())
                if command.strip().upper() == "QUIT":
                    break


if __name__ == "__main__":
    main()
