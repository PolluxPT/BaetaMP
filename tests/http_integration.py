#!/usr/bin/env python3
"""Native libmpv/curl integration tests against an owned local HTTP server."""
import http.server
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import urllib.parse

root = Path(__file__).resolve().parent.parent
source = root / "build"
template = root / "APPS/BaetaMP"
body = (template / "Filmes/Sample_720p.mp4").read_bytes()
requests = []


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        requests.append((self.path, self.headers.get("User-Agent"), self.headers.get("Referer")))
        if self.path.startswith("/forbidden") or self.headers.get("User-Agent") != "BaetaTest 1.0" or self.headers.get("Referer") != "https://allowed.example/":
            self.send_response(403)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/base/list.m3u")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if self.path == "/base/list.m3u":
            data, mime = b"#EXTM3U\n#EXTINF:-1,Canal\ncanal.mp4\n", "audio/x-mpegurl"
        else:
            data, mime = body, "video/mp4"
        self.send_response(200)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass


env = dict(os.environ, SDL_VIDEODRIVER="offscreen", LIBGL_ALWAYS_SOFTWARE="1", ASAN_OPTIONS="detect_leaks=0", RGDS_RESTART_AFTER_VIDEO="1", NO_PROXY="127.0.0.1,localhost")
sdk = root / "sdk/root/usr/lib/x86_64-linux-gnu"
if sdk.exists():
    env["LD_LIBRARY_PATH"] = str(sdk)
server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
opts = "|User-Agent=BaetaTest%201.0&Referer=" + urllib.parse.quote("https://allowed.example/", safe="")
try:
    with tempfile.TemporaryDirectory(prefix="baetamedia-test-") as temp:
        app = Path(temp)
        shutil.copytree(template / "fonts", app / "fonts")
        (app / "config/retomar").mkdir(parents=True)
        for endpoint in ["/allowed.mp4", "/forbidden?private=DO_NOT_LOG"]:
            url = f"http://127.0.0.1:{server.server_port}{endpoint}{opts}"
            result = subprocess.run([str(source / "baetamp-native"), str(app), "--stress", url], env=env, capture_output=True, text=True, timeout=24)
            print(endpoint.split("?")[0], "exit", result.returncode, flush=True)
            print(result.stderr[-1600:], flush=True)
            assert result.returncode == 42
            assert "DO_NOT_LOG" not in result.stderr
            if endpoint == "/allowed.mp4":
                assert "FILE_LOADED" in result.stderr
                assert "HTTP compatibility: ua=0 headers=0 fields=1" in result.stderr
            else:
                assert "HTTP 403" in result.stderr
        url = f"http://127.0.0.1:{server.server_port}/redirect{opts}"
        result = subprocess.run([str(source / "http-native"), url], env=env, capture_output=True, text=True, timeout=24)
        print(result.stdout, result.stderr, flush=True)
        assert result.returncode == 0
        assert all(ua == "BaetaTest 1.0" and ref == "https://allowed.example/" for _, ua, ref in requests)
        print("HTTP video, playlist headers, redirects, and redacted 403 diagnosis: PASS", flush=True)
finally:
    server.shutdown()
