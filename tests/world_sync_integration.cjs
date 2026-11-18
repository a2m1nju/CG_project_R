// Build Release x64 first, then run: node tests/world_sync_integration.cjs [Server.exe]
const assert = require('node:assert/strict');
const { spawn } = require('node:child_process');
const net = require('node:net');
const path = require('node:path');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));

class Peer {
    constructor() {
        this.buffer = Buffer.alloc(0);
        this.packets = [];
        this.sequence = 0;
        this.socket = net.connect(9000, '127.0.0.1');
        this.socket.on('error', error => { this.error = error; });
        this.socket.on('data', bytes => {
            this.buffer = Buffer.concat([this.buffer, bytes]);
            while (this.buffer.length >= 2) {
                const size = this.buffer.readUInt16BE();
                assert(size >= 12 && size <= 1024);
                if (this.buffer.length < size) break;
                this.packets.push(this.buffer.subarray(0, size));
                this.buffer = this.buffer.subarray(size);
            }
        });
    }
    send(type, round, body = Buffer.alloc(0)) {
        const packet = Buffer.alloc(12 + body.length);
        packet.writeUInt16BE(packet.length);
        packet[2] = 1;
        packet[3] = type;
        packet.writeUInt32BE(++this.sequence, 4);
        packet.writeUInt32BE(round, 8);
        body.copy(packet, 12);
        this.socket.write(packet);
    }
    async until(predicate) {
        const deadline = Date.now() + 5000;
        while (!predicate()) {
            if (this.error) throw this.error;
            assert(Date.now() < deadline, 'packet timeout');
            await delay(10);
        }
    }
    async join() {
        await this.until(() => this.socket.readyState === 'open');
        this.send(1, 0, Buffer.from([4, ...Buffer.from('Peer')]));
        await this.until(() => this.packets.some(p => p[3] === 2));
        assert.equal(this.packets[0][3], 2, 'WELCOME precedes world data');
        this.playerId = this.packets[0].readUInt32BE(16);

    }
    lanes() {
        return this.packets.filter(p => p[3] === 20).slice(0, 41)
            .map(p => p.subarray(8).toString('hex'));
    }
    items() {
        return this.packets.filter(p => p[3] === 31)
            .map(p => p.subarray(8).toString('hex'));
    }
}

async function main() {
    const server = spawn(path.resolve(process.argv[2] || 'x64/Release/Server.exe'), [],
        { windowsHide: true });
    let output = '';
    server.stdout.on('data', bytes => { output += bytes; });
    server.stderr.on('data', bytes => { output += bytes; });
    const peers = [];
    try {
        await delay(300);
        assert.equal(server.exitCode, null, output);
        for (let i = 0; i < 3; ++i) {
            const peer = new Peer();
            peers.push(peer);
            await peer.join();
        }
        const [a, b, c] = peers;
        // Ready/cancel continues using the existing lobby protocol.
        for (const peer of peers) await peer.until(() => peer.packets.some(p => p[3] === 4));
        const ready = Buffer.alloc(5);
        ready.writeUInt32BE(a.packets.find(p => p[3] === 4).readUInt32BE(12));
        ready[4] = 1;
        a.send(5, 0, ready);
        for (const peer of peers) await peer.until(() => peer.packets.filter(p => p[3] === 4).length >= 2);
        ready[4] = 0;
        a.send(5, 0, ready);
        for (const peer of peers) await peer.until(() => peer.packets.filter(p => p[3] === 4).length >= 3);

        assert(peers.every(p => !p.packets.some(packet => packet[3] === 22)));
        ready[4] = 1;
        a.send(5, 0, ready);
        b.send(5, 0, ready);
        await delay(120);
        assert(peers.every(p => !p.packets.some(packet => packet[3] === 22)), 'two ready players cannot start loading');
        c.send(5, 0, ready);
        for (const peer of peers) {
            await peer.until(() => peer.packets.some(packet => packet[3] === 32));
            peer.round = peer.packets.find(packet => packet[3] === 22).readUInt32BE(8);
        }
        assert.equal(a.lanes().length, 41);
        assert(a.items().length >= 3);
        for (const peer of [b, c]) {
            assert.deepEqual(peer.lanes(), a.lanes());
            assert.deepEqual(peer.items(), a.items());
        }

        a.send(7, a.round); b.send(7, b.round);
        await delay(120);
        assert(peers.every(p => !p.packets.some(packet => packet[3] === 8)), 'all clients must finish loading');
        if (process.argv.includes('--cancel-loading')) {
        c.socket.destroy();
        await a.until(() => a.packets.some(packet => packet[3] === 3));
        await b.until(() => b.packets.some(packet => packet[3] === 3));
        const d = new Peer(); peers.push(d); await d.join();
        for (const peer of [a,b,d]) await peer.until(() => peer.packets.some(packet => packet[3] === 4 && packet.readUInt32BE(12) > ready.readUInt32BE()));
        ready.writeUInt32BE(d.packets.find(packet => packet[3] === 4).readUInt32BE(12));
        ready[4] = 1;
        for (const peer of [a,b,d]) peer.send(5,0,ready);
        for (const peer of [a,b,d]) {
            await peer.until(() => peer.packets.some(packet => packet[3] === 32 && packet.readUInt32BE(8) > a.round));
            peer.send(7,a.round+1);
        }
        for (const peer of [a,b,d]) await peer.until(() => peer.packets.some(packet => packet[3] === 8 && packet.readUInt32BE(8) === a.round+1));
        console.log('PASS: loading disconnect, queue recovery, replacement member, fresh round start');
            return;
        }
        c.send(7, c.round);
        for (const peer of peers) await peer.until(() => peer.packets.some(packet => packet[3] === 8));

        // Spawn a shield on a safe tile, then pick it up. All clients see the same object.
        a.send(42, a.round, Buffer.from([4]));
        for (const peer of peers) await peer.until(() => peer.packets.some(p => p[3] === 43));
        const spawned = a.packets.find(p => p[3] === 43);
        for (const peer of [b, c])
            assert.deepEqual(peer.packets.find(p => p[3] === 43).subarray(8), spawned.subarray(8));
        const position = Buffer.alloc(9);
        position.writeFloatBE(spawned.readInt32BE(17), 0);
        position.writeFloatBE(spawned.readInt32BE(21), 4);
        position[8] = 1;
        a.send(11, a.round, position);
        a.send(33, a.round, spawned.subarray(12, 16));
        for (const peer of peers) await peer.until(() => peer.packets.some(p => p[3] === 34));
        const removed = a.packets.find(p => p[3] === 34);
        assert.equal(removed.readUInt32BE(12), spawned.readUInt32BE(12));
        assert.equal(removed.readUInt32BE(16), a.playerId);
        a.send(37, a.round);
        await a.until(() => a.packets.some(p => p[3] === 39 && p[12] === 4));

        // A shared reset requires every joined player, and replaces maps and items for everyone.
        for (const peer of peers) peer.send(12, a.round + 1);
        for (const peer of peers)
            await peer.until(() => peer.packets.some(p => p[3] === 32 && p.readUInt32BE(8) === a.round + 1));
        for (const peer of peers) peer.send(7, a.round + 1);
        for (const peer of peers) await peer.until(() => peer.packets.some(p => p[3] === 8 && p.readUInt32BE(8) === a.round + 1));
        for (const peer of peers) assert(!peer.packets.some(p => p[3] === 44));
        console.log('PASS: identical maps/items, ready/cancel, item spawn/pickup/shield, shared reset');
    } finally {
        for (const peer of peers) peer.socket.destroy();
        if (server.exitCode === null) {
            server.stdin.end('\n');
            await Promise.race([
                new Promise(resolve => server.once('exit', resolve)),
                delay(5000).then(() => { if (server.exitCode === null) server.kill(); })
            ]);
        }
    }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
