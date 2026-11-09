"""Real TCP regression test. Build Server first; run: python tests/movement_integration.py [Server.exe]"""
import math
import re
import socket
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path


class Peer:
    def __init__(self):
        self.sock = socket.create_connection(('127.0.0.1', 9000), timeout=2)
        self.sock.settimeout(2)
        self.buffer = b''
        self.seq = 0

    def packet(self, kind, body):
        self.seq += 1
        return struct.pack('!HBBI', 8 + len(body), 1, kind, self.seq) + body

    def move(self, inp, direction, round_id=1):
        return self.packet(9, struct.pack('!IIB', round_id, inp, direction))

    def sync(self, x, z, alive=True, round_id=1):
        return self.packet(11, struct.pack('!IffB', round_id, x, z, alive))

    def state(self, predicate=lambda s: True):
        until = time.monotonic() + 3
        while time.monotonic() < until:
            while len(self.buffer) >= 2:
                size = struct.unpack('!H', self.buffer[:2])[0]
                if len(self.buffer) < size:
                    break
                packet, self.buffer = self.buffer[:size], self.buffer[size:]
                assert size == 29 and packet[2:4] == b'\x01\x0a'
                values = struct.unpack('!IIIIffB', packet[4:])
                s = dict(zip(('seq', 'round', 'input', 'tick', 'x', 'z', 'status'), values))
                if predicate(s):
                    return s
            data = self.sock.recv(4096)
            assert data, 'unexpected disconnect'
            self.buffer += data
        raise AssertionError('state timeout')


def main():
    exe = Path(sys.argv[1] if len(sys.argv) > 1 else 'x64/Debug/Server.exe').resolve()
    peers = []
    with tempfile.TemporaryFile() as log:
        server = subprocess.Popen([str(exe)], stdin=subprocess.PIPE, stdout=log, stderr=log)
        try:
            for attempt in range(80):
                try:
                    a = Peer()
                    break
                except OSError:
                    if server.poll() is not None:
                        raise AssertionError('server startup failed')
                    time.sleep(.025)
            else:
                raise AssertionError('server startup timeout')
            peers.append(a)
            initial = a.state()
            assert (initial['round'], initial['x'], initial['z']) == (1, 0, 0)

            # Fragmented frame must not move until the complete MOVE arrives.
            packet = a.move(1, 0)
            a.sock.sendall(packet[:5])
            s = a.state(lambda s: s['tick'] > initial['tick'] + 1)
            assert s['input'] == 0 and s['z'] == 0
            a.sock.sendall(packet[5:])
            first = a.state(lambda s: s['input'] == 1)
            assert (first['status'], first['x'], first['z']) == (1, 0, -1)
            a.sock.sendall(a.move(2, 3))
            denied = a.state(lambda s: s['input'] == 2)
            assert denied['status'] == 2 and denied['x'] == 0
            a.state(lambda s: s['tick'] >= first['tick'] + 6)
            a.sock.sendall(a.move(3, 3))
            moved = a.state(lambda s: s['input'] == 3)
            assert (moved['status'], moved['x'], moved['z']) == (1, 1, -1)

            # Duplicate input, wrong round, invalid direction cannot move a player.
            a.sock.sendall(a.move(3, 1) + a.move(4, 1, 99) + a.move(4, 255))
            invalid = a.state(lambda s: s['input'] == 4)
            assert (invalid['status'], invalid['x'], invalid['z']) == (3, 1, -1)

            # Environmental displacement/death survives idle ticks; NaN is ignored.
            a.sock.sendall(a.sync(2.25, -8, False) + a.move(5, 0))
            dead = a.state(lambda s: s['input'] == 5)
            assert (dead['status'], dead['x'], dead['z']) == (3, 2.25, -8)
            a.sock.sendall(a.sync(math.nan, 0))
            assert a.state(lambda s: s['tick'] > dead['tick'])['x'] == 2.25

            # Reset and an old-round MOVE coalesced in one TCP write.
            a.sock.sendall(a.packet(12, struct.pack('!I', 2)) + a.move(6, 0, 1))
            reset = a.state(lambda s: s['round'] == 2)
            assert (reset['input'], reset['x'], reset['z']) == (0, 0, 0)
            a.sock.sendall(a.move(1, 2, 2))
            assert a.state(lambda s: s['round'] == 2 and s['input'] == 1)['x'] == -1

            # Simultaneous clients have independent positions and sequence spaces.
            b, c = Peer(), Peer()
            peers.extend((b, c))
            b.state(); c.state()
            b.sock.sendall(b.move(1, 1)); c.sock.sendall(c.move(1, 3))
            bs, cs = b.state(lambda s: s['input'] == 1), c.state(lambda s: s['input'] == 1)
            assert (bs['x'], bs['z'], cs['x'], cs['z']) == (0, 1, 1, 0)
            # Malformed length closes only that peer; another keeps receiving ticks.
            c.sock.sendall(b'\x00\x01')
            while c.sock.recv(4096):
                pass
            assert b.state(lambda s: s['tick'] > bs['tick'])['z'] == 1
            b.sock.close()
            d = Peer(); peers.append(d)
            assert d.state()['x'] == 0
            if len(sys.argv) > 2:
                subprocess.run([str(Path(sys.argv[2]).resolve())], check=True, timeout=10)
            # Every server tick must emit coordinates, not just every thirtieth tick.
            time.sleep(.15)
        finally:
            for peer in peers:
                peer.sock.close()
            if server.poll() is None:
                server.communicate(b'\n', timeout=5)
        assert server.returncode == 0
        log.seek(0)
        lines = log.read().decode(errors='replace')
        ticks = [int(t) for t in re.findall(r'\[TICK (\d+)\] player=1 ', lines)]
        assert len(ticks) >= 10 and all(b == a + 1 for a, b in zip(ticks, ticks[1:])), lines
        print('PASS: fragmentation, 30Hz states/logs, directions, cooldown, stale/invalid input,')
        print('      world/death sync, reset, three clients, malformed frame, reconnect, shutdown')


if __name__ == '__main__':
    main()
