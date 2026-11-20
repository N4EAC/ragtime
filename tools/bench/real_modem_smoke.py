#!/usr/bin/env python3
"""Opt-in release-host check over crossed audio FIFOs; no sound card or CAT.

Starts two data2g-host processes on isolated loopback ports, exchanges
synthetic session bytes in both directions, then disconnects. This checks
the external modem; Ragtime's protocol/UI tests are separate. Not in CI.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import select
import socket
import subprocess
import tempfile
import time


class Peer:
    def __init__(self, command, data, log):
        self.command, self.data, self.log = command, data, log
        self.lines = []
        self.partial = b""
        self.received = bytearray()

    def send(self, command):
        self.log.write(("> " + command + "\n").encode())
        self.command.sendall((command + "\r").encode())

    def read(self, sock):
        data = sock.recv(65536)
        if not data:
            raise RuntimeError("A host closed its connection unexpectedly")
        if sock is self.data:
            self.received.extend(data)
            return
        self.log.write(data)
        self.log.flush()
        self.partial += data
        while b"\r" in self.partial:
            line, self.partial = self.partial.split(b"\r", 1)
            line = line.strip()
            if line:
                self.lines.append(line)
                if line.startswith((b"WRONG", b"FAILED")):
                    raise RuntimeError("Host rejected a command: " + line.decode(errors="replace"))


def reserve_check(ports):
    held = []
    try:
        for port in ports:
            s = socket.socket()
            held.append(s)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind(("127.0.0.1", port))
            s.listen(1)
    finally:
        for s in held:
            s.close()


def connect(port, processes):
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        if any(p.poll() is not None for p in processes):
            raise RuntimeError("A host exited during startup; inspect host logs")
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=0.5)
        except OSError:
            time.sleep(0.1)
    raise RuntimeError(f"Host did not listen on {port}")


def wait_for(peers, processes, predicate, timeout, description):
    sockets = {s: p for p in peers for s in (p.command, p.data)}
    deadline = time.monotonic() + timeout
    while not predicate():
        if time.monotonic() >= deadline:
            raise RuntimeError("Timed out: " + description)
        if any(p.poll() is not None for p in processes):
            raise RuntimeError("A host exited; inspect host logs")
        readable, _, _ = select.select(list(sockets), [], [], 0.1)
        for sock in readable:
            sockets[sock].read(sock)


def run(args, work):
    processes, sockets, files, peers = [], [], [], []
    fifos = [work / name for name in ("a-to-b", "b-to-a")]
    commands = []
    completed = False
    try:
        for fifo in fifos:
            os.mkfifo(fifo)
        for i, call in enumerate(("W1AAA", "W1BBB")):
            config = work / f"host-{i}.conf"
            config.write_text("")
            log = (work / f"host-{i}.log").open("wb")
            files.append(log)
            incoming, outgoing = (fifos[1], fifos[0]) if i == 0 else (fifos[0], fifos[1])
            command = [str(args.host), "--config", str(config), "--mycall", call,
                       "--listen", "127.0.0.1", "--kiss-port", str(args.kiss_port + i * 10),
                       "--command-port", str(args.command_port + i * 10),
                       "--audio-io", f"pipe:{incoming},{outgoing}",
                       "--no-rig", "--no-record", "--threads", "2"]
            commands.append(command)
            processes.append(subprocess.Popen(command, cwd=work, stdout=log, stderr=subprocess.STDOUT))
        for i in range(2):
            command = connect(args.command_port + i * 10, processes)
            sockets.append(command)
            data = connect(args.command_port + i * 10 + 1, processes)
            sockets.append(data)
            log = (work / f"commands-{i}.log").open("wb")
            files.append(log)
            peer = Peer(command, data, log)
            peers.append(peer)
            for line in (f"MYCALL {'W1AAA' if i == 0 else 'W1BBB'}", "CHAT ON",
                         "DATA2G ON", "LISTEN ON", "BW2300"):
                peer.send(line)
        wait_for(peers, processes, lambda: all(p.lines.count(b"OK") >= 5 for p in peers),
                 10, "host setup replies")
        for p in peers:
            p.lines.clear()
        peers[0].send("CONNECT W1AAA W1BBB")
        print("Waiting for the two real modems to connect", flush=True)
        wait_for(peers, processes,
                 lambda: all(any(line.startswith(b"CONNECTED ") for line in p.lines) for p in peers),
                 90, "ARQ connection")
        print("Both hosts connected", flush=True)
        for sender, receiver, payload in ((0, 1, "Ragtime preflight: café — 73\n".encode()),
                                          (1, 0, b"Reply from W1BBB: 73\n")):
            peers[sender].data.sendall(payload)
            wait_for(peers, processes, lambda: len(peers[receiver].received) >= len(payload),
                     60, "session payload")
            if bytes(peers[receiver].received) != payload:
                raise RuntimeError("Received session bytes differ from the test payload")
            peers[receiver].received.clear()
            print(f"{sender} -> {receiver}: {len(payload)} bytes identical", flush=True)
        for p in peers:
            p.lines.clear()
        peers[0].send("DISCONNECT")
        wait_for(peers, processes,
                 lambda: all(any(line.startswith(b"DISCONNECTED") for line in p.lines) for p in peers),
                 30, "session disconnect")
        completed = True
        return commands
    finally:
        for sock in sockets:
            sock.close()
        for process in processes:
            if process.poll() is None:
                process.terminate()
        cleanup_errors = []
        for process in processes:
            try:
                process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                cleanup_errors.append("A host needed forced termination")
            if process.returncode != 0:
                cleanup_errors.append(f"A host exited with status {process.returncode}")
        for file in files:
            file.close()
        for fifo in fifos:
            if fifo.exists():
                fifo.unlink()
        if completed and cleanup_errors:
            raise RuntimeError("; ".join(cleanup_errors))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", type=Path, required=True, help="Release data2g-host executable")
    parser.add_argument("--output", type=Path, default=Path("build-real-host-smoke"))
    parser.add_argument("--kiss-port", type=int, default=18100, help="Host A; host B uses this + 10")
    parser.add_argument("--command-port", type=int, default=18300, help="Host A; host B uses this + 10")
    args = parser.parse_args()
    args.host = args.host.resolve()
    if not hasattr(os, "mkfifo"):
        parser.error("This check needs POSIX named pipes (macOS/Linux)")
    if not args.host.is_file() or not os.access(args.host, os.X_OK):
        parser.error("--host must name an executable")
    ports = [p + i * 10 for i in range(2)
             for p in (args.kiss_port, args.command_port, args.command_port + 1)]
    if len(set(ports)) != 6 or any(p < 1024 or p > 65535 for p in ports):
        parser.error("Choose six distinct unprivileged ports in 1024..65535")
    try:
        reserve_check(ports)
    except OSError as error:
        parser.error(f"Bench ports unavailable: {error}; choose other --kiss-port/--command-port values")
    args.output.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="run-", dir=args.output)).resolve()
    if "," in str(work):
        parser.error("The host's pipe syntax cannot represent output paths containing commas")
    print("Evidence:", work, flush=True)
    evidence = {"host": str(args.host), "sha256": hashlib.sha256(args.host.read_bytes()).hexdigest()}
    try:
        evidence["commands"] = run(args, work)
        evidence["result"] = "pass"
        print("PASS: bidirectional modem bytes and disconnect; no audio devices or CAT", flush=True)
        return 0
    except (OSError, RuntimeError, KeyboardInterrupt) as error:
        evidence["result"] = "failed"
        evidence["error"] = str(error) or "Interrupted"
        print("FAIL:", evidence["error"], flush=True)
        return 1
    finally:
        (work / "result.json").write_text(json.dumps(evidence, indent=2) + "\n")


if __name__ == "__main__":
    raise SystemExit(main())
