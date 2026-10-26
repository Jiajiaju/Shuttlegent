"""Sample external MCP server for Shuttlegent (stdio): a tiny team glossary.

The project declares it in DefaultShuttlegent.ini as an external MCP server of type Stdio; the runtime starts it with
every session. Messages are newline-delimited JSON-RPC on stdin and stdout. Standard library only.
"""
import json
import sys

PROTOCOL_VERSION = "2025-06-18"
METHOD_NOT_FOUND = -32601

GLOSSARY = {
    "LI": "Level Instance: a level placed inside another level as one actor.",
    "PLA": "Packed Level Actor: a level instance baked into instanced static meshes.",
    "HLOD": "Hierarchical LOD: merged stand-in meshes World Partition shows for far cells.",
    "WP": "World Partition: streams a large world in grid cells instead of sublevels.",
}

TOOLS = [{
    "name": "lookup",
    "description": "Look up a term of the team glossary (sample external MCP server). Without a term, lists all terms.",
    "inputSchema": {"type": "object", "properties": {"term": {"type": "string", "description": "Term, e.g. HLOD."}}, "additionalProperties": False},
    "annotations": {"readOnlyHint": True},
}]


def answer(message):
    method = message.get("method")
    params = message.get("params") or {}
    if method == "initialize":
        return {"protocolVersion": params.get("protocolVersion", PROTOCOL_VERSION), "capabilities": {"tools": {"listChanged": False}}, "serverInfo": {"name": "shuttlegent-sample-glossary", "version": "0.1.0"}}
    if method == "tools/list":
        return {"tools": TOOLS}
    if method == "ping":
        return {}
    if method == "tools/call":
        term = (params.get("arguments") or {}).get("term", "")
        if not term:
            text = "\n".join(f"{key}: {value}" for key, value in GLOSSARY.items())
        else:
            text = GLOSSARY.get(term.upper(), f"No glossary entry for {term}.")
        return {"content": [{"type": "text", "text": text}], "isError": False}
    return None


for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    message = json.loads(line)
    if "id" not in message:
        continue
    result = answer(message)
    reply = {"jsonrpc": "2.0", "id": message["id"]}
    if result is None:
        reply["error"] = {"code": METHOD_NOT_FOUND, "message": f"Unknown method {message.get('method')}"}
    else:
        reply["result"] = result
    sys.stdout.write(json.dumps(reply, ensure_ascii=False) + "\n")
    sys.stdout.flush()
