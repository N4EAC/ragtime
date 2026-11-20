"""A real Cocoa app must release an active fake-modem session on exit.

Needs a logged-in desktop. Uses only a bench it starts itself; an occupied
bench port skips the test before the application is launched.
"""
import argparse
import os
from pathlib import Path
import pty
import socket
import subprocess
import tempfile
import time


def stop(process):
    if process is not None and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def check(app_path, bench_path):
    with tempfile.TemporaryDirectory(prefix="ragtime-shutdown-") as folder:
        root = Path(folder)
        config = root / "station.conf"
        config.write_text("[Station]\nCallsign=MACOSQA\n[Connection]\n"
                          "Host=127.0.0.1\nKissPort=8100\nCommandPort=8300\n"
                          f"[Files]\nReceivedFolder={root / 'received'}\n")
        master, slave = pty.openpty()
        bench = app = None
        output = bytearray()
        try:
            bench = subprocess.Popen([bench_path, "--bots", "1", "--call", "MACOSQA",
                                      "--airtime", "100"], stdout=slave, stderr=slave)
            os.close(slave)
            slave = None
            os.set_blocking(master, False)
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                try:
                    output.extend(os.read(master, 65536))
                except (BlockingIOError, OSError):
                    pass
                if b"Fake data2g-hosts on 127.0.0.1" in output:
                    break
                if bench.poll() is not None:
                    if b"port taken" in output:
                        print("Bench ports occupied: skipped")
                        return 77
                    raise RuntimeError(f"Bench failed: {output.decode(errors='replace')}")
                time.sleep(0.05)
            else:
                raise RuntimeError("Bench did not become ready")

            with (root / "app.log").open("w+") as app_log:
                app = subprocess.Popen([app_path, "--config", str(config)],
                                       stdout=app_log, stderr=subprocess.STDOUT)
                deadline = time.monotonic() + 50
                while time.monotonic() < deadline:
                    logs = list(root.glob("station.conf.flight/*.log"))
                    if any("cmd< CONNECTED " in p.read_text() for p in logs):
                        break
                    if app.poll() is not None:
                        app_log.seek(0)
                        raise RuntimeError(f"App exited before the session: {app_log.read()}")
                    time.sleep(0.1)
                else:
                    raise RuntimeError("App did not enter the incoming session")

                # SIGTERM uses the same close handler as the close box and
                # Cmd+Q. Leave the bench running to expose a stale ARQ link.
                app.terminate()
                app.wait(timeout=10)
                if app.returncode != 0:
                    raise RuntimeError(f"App quit with status {app.returncode}")

            # A new client must be able to call the same station. Before the
            # fix, the bench immediately answered DISCONNECTED here because
            # closing the client sockets had left its partner attached.
            with socket.create_connection(("127.0.0.1", 8300), timeout=5) as probe:
                probe.settimeout(5)
                probe.sendall(b"MYCALL MACOSQA\rDATA2G ON\rLISTEN ON\rCONNECT MACOSQA W1AW\r")
                replies = bytearray()
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    chunk = probe.recv(65536)
                    if not chunk:
                        raise RuntimeError("Bench closed the fresh client's command socket")
                    replies.extend(chunk)
                    if b"CONNECTED MACOSQA W1AW" in replies:
                        probe.sendall(b"ABORT\r")
                        print("Cocoa exit released the session; a new client called successfully")
                        return 0
                    if b"DISCONNECTED" in replies:
                        raise RuntimeError("The modem session remained attached after app exit")
                raise RuntimeError(f"No answer to the new call: {replies!r}")
        finally:
            stop(app)
            stop(bench)
            os.close(master)
            if slave is not None:
                os.close(slave)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True)
    parser.add_argument("--bench", required=True)
    args = parser.parse_args()
    raise SystemExit(check(args.app, args.bench))
