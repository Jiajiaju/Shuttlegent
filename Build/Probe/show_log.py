"""Print a compact view of an acp_probe jsonl log.

Usage: python -I show_log.py <log.jsonl> [--messages]
  default     protocol timeline; streamed text chunks are joined
  --messages  only the assistant's final message text
"""
import json
import sys


def iter_events(path):
    with open(path, encoding="utf-8") as f:
        for line in f:
            yield json.loads(line)


def print_messages(path):
    out = []
    for e in iter_events(path):
        d = e["data"]
        if e["kind"] == "recv" and isinstance(d, dict) and d.get("method") == "session/update":
            u = d["params"]["update"]
            if u.get("sessionUpdate") == "agent_message_chunk":
                out.append(u["content"].get("text", ""))
    print("".join(out))


def print_timeline(path):
    text = []

    def flush(t):
        if text:
            print(f'{t:>8} TEXT {"".join(text)[:800]!r}')
            text.clear()

    for e in iter_events(path):
        kind, data = e["kind"], e["data"]
        if kind == "recv" and isinstance(data, dict) and data.get("method") == "session/update":
            upd = data["params"]["update"]
            su = upd.get("sessionUpdate")
            if su in ("agent_message_chunk", "agent_thought_chunk"):
                text.append(("[think]" if su == "agent_thought_chunk" else "") + upd.get("content", {}).get("text", ""))
                continue
            flush(e["t"])
            brief = {k: v for k, v in upd.items() if k != "content"}
            if "content" in upd:
                brief["content"] = json.dumps(upd["content"], ensure_ascii=False)[:300]
            print(f'{e["t"]:>8} UPDATE {json.dumps(brief, ensure_ascii=False)[:500]}')
            continue
        flush(e["t"])
        print(f'{e["t"]:>8} {kind} {json.dumps(data, ensure_ascii=False)[:400]}')
    if text:
        print("TEXT", "".join(text)[:800])


if __name__ == "__main__":
    if "--messages" in sys.argv[2:]:
        print_messages(sys.argv[1])
    else:
        print_timeline(sys.argv[1])
