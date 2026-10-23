"""Minimal Streamable HTTP MCP server that mimics the Shuttlegent editor endpoint.

Usage: python -I mcp_mock.py <port> <token> <log_path>
Responds with plain JSON (no SSE). Logs every request to log_path as JSON lines.
"""
import json
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PORT = int(sys.argv[1])
TOKEN = sys.argv[2]
LOG_PATH = sys.argv[3]
LOCK = threading.Lock()

TOOLS = [
    {
        "name": "ue_read_probe",
        "description": "Read-only probe. Returns the current editor level name.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
        "annotations": {"readOnlyHint": True},
    },
    {
        "name": "ue_write_probe",
        "description": "Write probe. Pretends to rename an actor in the level.",
        "inputSchema": {
            "type": "object",
            "properties": {"actor": {"type": "string"}, "newName": {"type": "string"}},
            "required": ["actor", "newName"],
            "additionalProperties": False,
        },
        "annotations": {"readOnlyHint": False, "destructiveHint": True},
    },
]


def log(entry):
    with LOCK:
        with open(LOG_PATH, "a", encoding="utf-8") as f:
            f.write(json.dumps(entry, ensure_ascii=False) + "\n")


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def _send(self, code, body=None, headers=None):
        data = b"" if body is None else json.dumps(body).encode("utf-8")
        self.send_response(code)
        if body is not None:
            self.send_header("Content-Type", "application/json")
        for k, v in (headers or {}).items():
            self.send_header(k, v)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if data:
            self.wfile.write(data)

    def do_GET(self):
        log({"method": "GET", "path": self.path})
        self._send(405)

    def do_DELETE(self):
        log({"method": "DELETE", "path": self.path})
        self._send(200)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length).decode("utf-8")
        auth = self.headers.get("Authorization", "")
        try:
            msg = json.loads(raw)
        except json.JSONDecodeError:
            msg = None
        log({"method": "POST", "scenario": self.headers.get("X-Probe-Scenario"), "auth_ok": auth == f"Bearer {TOKEN}", "accept": self.headers.get("Accept"), "body": msg})
        if auth != f"Bearer {TOKEN}":
            self._send(401, {"error": "unauthorized"})
            return
        if not isinstance(msg, dict):
            self._send(400, {"error": "batch or invalid body not supported"})
            return
        if "id" not in msg:
            self._send(202)
            return
        rpc_id = msg["id"]
        method = msg.get("method")
        if method == "initialize":
            result = {
                "protocolVersion": msg.get("params", {}).get("protocolVersion", "2025-06-18"),
                "capabilities": {"tools": {"listChanged": False}},
                "serverInfo": {"name": "shuttlegent-mock", "version": "0.0.1"},
                "instructions": "Mock Unreal Editor tools for Shuttlegent M0.",
            }
        elif method == "tools/list":
            result = {"tools": TOOLS}
        elif method == "tools/call":
            name = msg.get("params", {}).get("name")
            args = msg.get("params", {}).get("arguments", {})
            text = "Level: /Game/Maps/M0_Test" if name == "ue_read_probe" else f"Renamed {args.get('actor')} to {args.get('newName')}"
            result = {"content": [{"type": "text", "text": text}], "isError": False}
        elif method == "ping":
            result = {}
        else:
            self._send(200, {"jsonrpc": "2.0", "id": rpc_id, "error": {"code": -32601, "message": f"unknown method {method}"}})
            return
        self._send(200, {"jsonrpc": "2.0", "id": rpc_id, "result": result}, {"Mcp-Session-Id": "m0-session"})


if __name__ == "__main__":
    ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
