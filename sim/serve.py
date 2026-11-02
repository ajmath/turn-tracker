#!/usr/bin/env python3
"""Loopback-only UI adapter for the native C++ simulated table; no dependencies."""
import argparse
import atexit
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import threading

ROOT = Path(__file__).resolve().parents[1]


class Engine:
    def __init__(self):
        self.process = subprocess.Popen([str(ROOT / 'build/table-ui')], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, text=True, bufsize=1)
        self.lock = threading.Lock()

    def command(self, command):
        if not isinstance(command, str) or len(command) > 100 or '\n' in command or '\r' in command:
            raise ValueError('Invalid command')
        with self.lock:
            self.process.stdin.write(command + '\n')
            self.process.stdin.flush()
            line = self.process.stdout.readline()
            if not line:
                raise RuntimeError('Simulation process exited; restart scripts/ui')
            return json.loads(line)

    def close(self):
        self.process.terminate()
        try:
            self.process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=8765)
    args = parser.parse_args()
    engine = Engine()
    atexit.register(engine.close)

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_args):
            pass

        def send(self, status, body, content_type):
            self.send_response(status)
            self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Content-Security-Policy', "default-src 'self'; style-src 'self'; script-src 'self'; connect-src 'self'; frame-ancestors 'none'")
            self.end_headers()
            self.wfile.write(body)

        def allowed(self):
            hosts = {f'127.0.0.1:{self.server.server_port}', f'localhost:{self.server.server_port}'}
            host = self.headers.get('Host')
            origin = self.headers.get('Origin')
            return host in hosts and (not origin or origin == 'http://' + host)

        def do_GET(self):
            if not self.allowed():
                return self.send(403, b'Local requests only', 'text/plain')
            files = {'/': ('index.html', 'text/html; charset=utf-8'),
                     '/app.js': ('app.js', 'text/javascript; charset=utf-8'),
                     '/style.css': ('style.css', 'text/css; charset=utf-8')}
            if self.path not in files:
                return self.send(404, b'Not found', 'text/plain')
            name, content_type = files[self.path]
            self.send(200, (ROOT / 'sim/ui' / name).read_bytes(), content_type)

        def do_POST(self):
            if not self.allowed() or self.headers.get('Content-Type') != 'application/json':
                return self.send(403, b'Local JSON requests only', 'text/plain')
            if self.path != '/api/command':
                return self.send(404, b'Not found', 'text/plain')
            try:
                length = int(self.headers.get('Content-Length', '0'))
                if not 0 < length <= 1024:
                    raise ValueError('Request is too large or empty')
                request = json.loads(self.rfile.read(length))
                result = engine.command(request['command'])
                self.send(200, json.dumps(result).encode(), 'application/json')
            except (ValueError, KeyError, TypeError) as error:
                self.send(400, json.dumps({'error': str(error)}).encode(), 'application/json')
            except (RuntimeError, BrokenPipeError) as error:
                self.send(503, json.dumps({'error': str(error)}).encode(), 'application/json')

    server = ThreadingHTTPServer(('127.0.0.1', args.port), Handler)
    print(f'Turn tracker UI: http://127.0.0.1:{server.server_port}', flush=True)
    print('One shared in-memory table. Ctrl-C stops it. Power cycles preserve each simulated disk; reset clears all.', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        engine.close()


if __name__ == '__main__':
    main()
