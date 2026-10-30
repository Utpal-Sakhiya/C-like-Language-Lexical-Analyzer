"""
server.py  -  Local web server for the Lexical Analyzer UI

Serves ui/index.html and forwards source code to the compiled C++ lexer
(lexer.exe --json), so the browser shows the output of the real analyzer.

    python ui/server.py            # then open http://localhost:8000
    python ui/server.py 9000       # use a different port

Uses only the Python standard library.
"""
import json
import os
import subprocess
import sys
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

UI_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(UI_DIR)
SAMPLES = {
    "valid": os.path.join(ROOT, "samples", "valid_program.c"),
    "errors": os.path.join(ROOT, "samples", "program_with_errors.c"),
}


def find_lexer():
    for name in ("lexer.exe", "lexer"):
        path = os.path.join(ROOT, name)
        if os.path.isfile(path):
            return path
    return None


def run_lexer(code, args):
    lexer = find_lexer()
    if lexer is None:
        raise RuntimeError("lexer executable not found - build it first (build.bat or make)")
    proc = subprocess.run([lexer, "-", *args], input=code.encode("utf-8"),
                          capture_output=True, timeout=10)
    if proc.returncode not in (0, 1):   # 1 just means lexical errors were found
        raise RuntimeError(proc.stderr.decode("utf-8", "replace") or "lexer failed")
    return proc.stdout.decode("utf-8", "replace")


class Handler(BaseHTTPRequestHandler):
    def send(self, status, body, content_type):
        data = body.encode("utf-8") if isinstance(body, str) else body
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def send_json(self, status, obj):
        self.send(status, json.dumps(obj), "application/json; charset=utf-8")

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            with open(os.path.join(UI_DIR, "index.html"), "rb") as f:
                self.send(200, f.read(), "text/html; charset=utf-8")
        elif self.path == "/api/samples":
            samples = {}
            for key, path in SAMPLES.items():
                with open(path, encoding="utf-8") as f:
                    samples[key] = f.read()
            self.send_json(200, samples)
        else:
            self.send(404, "Not found", "text/plain")

    def do_POST(self):
        try:
            length = int(self.headers.get("Content-Length", 0))
            code = json.loads(self.rfile.read(length) or b"{}").get("code", "")
            if self.path == "/api/analyze":
                self.send(200, run_lexer(code, ["--json"]), "application/json; charset=utf-8")
            elif self.path == "/api/report":
                report = run_lexer(code, ["--no-color"])
                self.send(200, "C-Like Language Lexical Analyzer - Analysis Report\n" + report,
                          "text/plain; charset=utf-8")
            else:
                self.send(404, "Not found", "text/plain")
        except Exception as exc:  # report any failure to the page instead of dropping the request
            self.send_json(500, {"error": str(exc)})

    def log_message(self, fmt, *args):
        pass  # keep the console quiet


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    if find_lexer() is None:
        print("Warning: lexer executable not found in", ROOT, "- run build.bat first.")
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    url = f"http://localhost:{port}"
    print(f"Lexical Analyzer UI running at {url}  (Ctrl+C to stop)")
    threading.Timer(0.5, lambda: webbrowser.open(url)).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")


if __name__ == "__main__":
    main()
