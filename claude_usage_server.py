#!/usr/bin/env python3
"""Tiny HTTP server that serves ClaudeUsage.csv to the ESP32-S3-BOX-3.

The ESP firmware polls this server every ~30 seconds and renders the
latest row in a dedicated "Claude" LVGL tab. Run on the PC that owns
the CSV file. Default CSV path is ../../ClaudeUsage.csv relative to
this script (i.e. the workspace root).

Usage:
    python claude_usage_server.py                   # default port 8765
    python claude_usage_server.py --port 9000
    python claude_usage_server.py --csv D:/other/Claude.csv
"""
import argparse
import http.server
import socketserver
import sys
from pathlib import Path

DEFAULT_CSV = Path(__file__).resolve().parents[2] / "ClaudeUsage.csv"
DEFAULT_PORT = 8765
DEFAULT_LOG = Path(__file__).resolve().parent / "claude_usage_server.log"

# Under pythonw.exe (e.g. Windows Task Scheduler with no console),
# sys.stdout/stderr are None. Any write() then raises AttributeError
# mid-request and the client sees "empty reply from server". Redirect
# both streams to a log file before any handler tries to log.
if sys.stdout is None or sys.stderr is None:
    _log_fp = open(DEFAULT_LOG, "a", buffering=1, encoding="utf-8")
    sys.stdout = _log_fp
    sys.stderr = _log_fp


class CsvHandler(http.server.BaseHTTPRequestHandler):
    csv_path: Path = DEFAULT_CSV

    def do_GET(self):
        if self.path not in ("/", "/ClaudeUsage.csv"):
            self.send_error(404)
            return
        try:
            data = self.csv_path.read_bytes()
        except FileNotFoundError:
            self.send_error(404, f"{self.csv_path} not found")
            return
        self.send_response(200)
        self.send_header("Content-Type", "text/csv; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, fmt, *args):
        # One concise line per request; suppress the default verbose form.
        msg = fmt % args
        sys.stdout.write(f"[{self.log_date_time_string()}] "
                         f"{self.address_string()} {msg}\n")
        sys.stdout.flush()


class ReuseAddrServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT,
                        help=f"TCP port (default {DEFAULT_PORT})")
    parser.add_argument("--bind", default="0.0.0.0",
                        help="bind address (default 0.0.0.0)")
    parser.add_argument("--csv", type=Path, default=DEFAULT_CSV,
                        help=f"path to ClaudeUsage.csv (default {DEFAULT_CSV})")
    args = parser.parse_args()

    CsvHandler.csv_path = args.csv.resolve()
    if not CsvHandler.csv_path.exists():
        print(f"warning: {CsvHandler.csv_path} does not exist yet",
              file=sys.stderr)

    with ReuseAddrServer((args.bind, args.port), CsvHandler) as srv:
        print(f"serving {CsvHandler.csv_path} on "
              f"http://{args.bind}:{args.port}/ClaudeUsage.csv")
        try:
            srv.serve_forever()
        except KeyboardInterrupt:
            print("\nstopped")


if __name__ == "__main__":
    main()
