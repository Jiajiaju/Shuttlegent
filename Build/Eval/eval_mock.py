"""Mock Shuttlegent editor MCP endpoint for the tool-choice evaluation.

Serves the tools of tools.snapshot.json (names, descriptions and schemas as the editor sends them) and answers every
call from world.json. Each task (the X-Probe-Scenario header) gets its own copy of the world, and write tools change
that copy, so an agent that checks its own change sees it. Every tool call is logged as one JSON line with the task id.

Usage: python -I eval_mock.py <port> <token> <log_path>
"""
import copy
import json
import os
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PORT = int(sys.argv[1])
TOKEN = sys.argv[2]
LOG_PATH = sys.argv[3]
HERE = os.path.dirname(os.path.abspath(__file__))
LOCK = threading.Lock()

with open(os.path.join(HERE, "tools.snapshot.json"), encoding="utf-8") as snapshot_file:
    SNAPSHOT = json.load(snapshot_file)
with open(os.path.join(HERE, "world.json"), encoding="utf-8") as world_file:
    INITIAL_WORLD = json.load(world_file)
WORLDS = {}

TOOLS = [{"name": t["name"], "description": t["description"], "inputSchema": t["inputSchema"],
          "annotations": {"readOnlyHint": t["readOnly"], "destructiveHint": not t["readOnly"]}} for t in SNAPSHOT["tools"]]


def strings(value):
    """Every string inside an argument value, lower-cased, for loose matching."""
    if isinstance(value, str):
        return [value.lower()]
    if isinstance(value, dict):
        return [s for v in value.values() for s in strings(v)]
    if isinstance(value, list):
        return [s for v in value for s in strings(v)]
    return []


def mentions(args, name):
    name = name.lower()
    return any(name in s or s.split("/")[-1].split(".")[0] == name.split("/")[-1] for s in strings(args))


class World:
    def __init__(self, data):
        self.data = data
        self.history = []

    def actor_line(self, actor):
        return f"{actor['label']} | {actor['class']} | {self.data['level']}:PersistentLevel.{actor['label']} | location {actor['location']}"

    def actors_named(self, values):
        return [a for a in self.data["actors"] if mentions(values, a["label"])]

    def assets_named(self, args):
        return [a for a in self.data["assets"] if mentions(args, a["path"].split("/")[-1]) or mentions(args, a["path"])]

    def data_table(self, args):
        return next((a for a in self.data["assets"] if a["class"] == "DataTable" and mentions(args, a["path"].split("/")[-1])), None)

    def call_toolset_tool(self, args):
        tool = str(args.get("tool", ""))
        inner = args.get("arguments") or {}
        # A broken asset, as after an undo destroyed it in the editor: every tool that touches it fails
        broken = next((a for a in self.data["assets"] if a.get("broken") and mentions(inner, a["path"].split("/")[-1])), None)
        if broken is not None:
            return (f"Error: Parameter error: {broken['path']}.{broken['path'].split('/')[-1]} has been destroyed and cannot be used "
                    "for property 'data_table'; query for a current object instead of reusing this path.")
        if str(args.get("toolset", "")).endswith("AssetTools"):
            if tool == "is_dirty":
                return '{"returnValue": false}'
            return '{"returnValue": true}'
        if not str(args.get("toolset", "")).endswith("DataTableTools"):
            return "Done."
        table = self.data_table(inner)
        if table is None:
            return "Error: data_table not found."
        rows = table["rows"]
        if tool in ("list_rows", "get_rows"):
            return json.dumps(rows)
        if tool == "get_schema":
            return '{"Name": "string", "Price": "int", "Icon": "SoftObjectPath"}'
        names = [n for n in inner.get("row_names", []) if isinstance(n, str)]
        if tool == "add_rows":
            existing = [n for n in names if n in rows]
            if existing:
                return f"Error: rows already exist: {existing}"
            rows.extend(names)
            self.history.append(f"Add rows to {table['path']}")
            return f"Added {names} to {table['path']}."
        if tool == "remove_rows":
            table["rows"] = [r for r in rows if r not in names]
            self.history.append(f"Remove rows from {table['path']}")
            return f"Removed {names}."
        return "Done."

    def answer(self, name, args):
        data = self.data
        if name == "list_actors":
            return f"Level {data['level']}, {len(data['actors'])} actors:\n" + "\n".join(self.actor_line(a) for a in data["actors"])
        if name == "get_selection":
            return "Selected actors:\n" + "\n".join(self.actor_line(a) for a in data["actors"] if a["label"] in data["selection"])
        if name == "find_assets":
            hits = [a for a in data["assets"] if any(s in a["path"].lower() or s == a["class"].lower() for s in strings(args))] or data["assets"]
            return "\n".join(f"{a['path']} | {a['class']}" for a in hits)
        if name == "get_asset":
            hits = self.assets_named(args)
            return "\n".join(f"{a['path']} | class {a['class']} | referencers {len(a.get('referencedBy', []))}" for a in hits) or "Asset not found."
        if name == "get_asset_references":
            hits = self.assets_named(args)
            if not hits:
                return "Asset not found."
            if "dependencies" in strings(args):
                return f"{hits[0]['path']} depends on: /Engine/EngineMaterials/DefaultMaterial"
            return f"Packages referencing {hits[0]['path']}:\n" + ("\n".join(hits[0].get("referencedBy", [])) or "none")
        if name == "get_properties":
            lines = [f"{a['label']} ({a['class']}): " + json.dumps(a.get("properties", {})) for a in self.actors_named(args)]
            lines += [f"{a['path']} ({a['class']}): " + json.dumps(a.get("properties", {})) for a in self.assets_named(args)]
            return "\n".join(lines) or "No object matched."
        if name == "describe_class":
            if mentions(args, "BP_Hero"):
                hero = next(a for a in data["assets"] if a["path"].endswith("BP_Hero"))
                return "BP_Hero_C : Character : Pawn : Actor\nEditable properties:\n" + "\n".join(f"{k} (float)" for k in hero["properties"])
            if mentions(args, "PointLight"):
                return "PointLight : Light : Actor\nEditable properties (on PointLightComponent):\nIntensity (float)\nLightColor (Color)\nAttenuationRadius (float)"
            return "Class not found."
        if name == "get_settings":
            return json.dumps(data["settings"], ensure_ascii=False)
        if name == "read_log":
            return "\n".join(data["log"])
        if name == "get_cvar":
            hits = {k: v for k, v in data["cvars"].items() if any(k.lower() in s or s in k.lower() for s in strings(args))}
            return "\n".join(f"{k} = {v}" for k, v in hits.items()) or "No console variable matched."
        if name == "list_toolsets":
            return "\n".join(f"{t['name']} ({len(t['tools'])} tools): {t['description']}" for t in data["toolsets"])
        if name == "describe_toolset":
            hits = [t for t in data["toolsets"] if mentions(args, t["name"])]
            if not hits:
                return "Toolset not found."
            if hits[0]["name"].endswith("DataTableTools"):
                return (f"{hits[0]['name']}\n"
                        "list_rows (read): Lists row names. Args: data_table (asset path)\n"
                        "add_rows (write): Adds empty rows. Args: data_table (asset path), row_names (string array)\n"
                        "set_rows (write): Sets row values from JSON. Args: data_table (asset path), values (JSON string)")
            return f"{hits[0]['name']}\n" + "\n".join(hits[0]["tools"])
        if name == "call_toolset_tool":
            return self.call_toolset_tool(args)
        if name == "set_property":
            targets = self.actors_named(args.get("targets", []))
            if not targets:
                return "Error: no target matched."
            for actor in targets:
                actor.setdefault("properties", {})[str(args.get("property"))] = args.get("value")
            self.history.append(f"Set {args.get('property')}")
            return f"Set {args.get('property')} to {args.get('value')} on {', '.join(a['label'] for a in targets)}. Undoable."
        if name == "spawn_actor":
            cls = str(args.get("class", "Actor")).split("/")[-1].split(".")[-1]
            label = str(args.get("label") or f"{cls}_{sum(a['class'] == cls for a in data['actors']) + 1}")
            data["actors"].append({"label": label, "class": cls, "location": args.get("location", [0, 0, 0])})
            self.history.append(f"Spawn {label}")
            return f"Spawned {cls} as {label} at {args.get('location', [0, 0, 0])}. Undoable."
        if name == "delete_actors":
            targets = self.actors_named(args.get("targets", []))
            data["actors"] = [a for a in data["actors"] if a not in targets]
            self.history.append("Delete actors")
            return f"Deleted {', '.join(a['label'] for a in targets) or 'nothing'}. Undoable."
        if name == "save_assets":
            return f"Saved {data['level']}."
        if name in ("undo", "redo"):
            step = self.history[-1] if self.history else "Set Intensity on PointLight_Hall"
            return f"{name.capitalize()}: {step}."
        return f"Unknown tool {name}."


def world_of(task):
    with LOCK:
        if task not in WORLDS:
            WORLDS[task] = World(copy.deepcopy(INITIAL_WORLD))
        return WORLDS[task]


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
        self._send(405)

    def do_DELETE(self):
        self._send(200)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        try:
            msg = json.loads(self.rfile.read(length).decode("utf-8"))
        except json.JSONDecodeError:
            msg = None
        if self.headers.get("Authorization", "") != f"Bearer {TOKEN}":
            self._send(401, {"error": "unauthorized"})
            return
        if not isinstance(msg, dict):
            self._send(400, {"error": "batch or invalid body not supported"})
            return
        if "id" not in msg:
            self._send(202)
            return
        method = msg.get("method")
        params = msg.get("params", {})
        if method == "initialize":
            result = {"protocolVersion": params.get("protocolVersion", "2025-06-18"), "capabilities": {"tools": {"listChanged": False}},
                      "serverInfo": SNAPSHOT["serverInfo"], "instructions": SNAPSHOT["instructions"]}
        elif method == "tools/list":
            result = {"tools": TOOLS}
        elif method == "tools/call":
            task = self.headers.get("X-Probe-Scenario")
            name = params.get("name")
            args = params.get("arguments") or {}
            log({"task": task, "tool": name, "arguments": args})
            result = {"content": [{"type": "text", "text": world_of(task).answer(name, args)}], "isError": False}
        elif method == "ping":
            result = {}
        else:
            self._send(200, {"jsonrpc": "2.0", "id": msg["id"], "error": {"code": -32601, "message": f"unknown method {method}"}})
            return
        self._send(200, {"jsonrpc": "2.0", "id": msg["id"], "result": result}, {"Mcp-Session-Id": "eval-session"})


if __name__ == "__main__":
    ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
