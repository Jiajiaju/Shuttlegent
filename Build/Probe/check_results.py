"""Evaluate RunRegression.ps1 output against the expected M0 behaviour.

Usage: python -I check_results.py <out_dir>
Exit code 0 when every executed check passes; scenarios without a log are reported as SKIP.
"""
import json
import os
import sys

OUT = sys.argv[1]
WORKSPACE = os.path.join(OUT, "ws")
MCP_LOG = os.path.join(OUT, "mcp_mock.log")

# s10_parallel: two prompts sent together count as run side by side when the later one finishes before this multiple of the earlier one's time
PARALLEL_FINISH_RATIO = 1.5
with open(os.path.join(OUT, "mode.json"), encoding="utf-8") as mode_file:
    SPLIT_LAUNCH_DIR = json.load(mode_file).get("splitLaunchDir", False)


def load(name):
    path = os.path.join(OUT, f"{name}.jsonl")
    if not os.path.exists(path):
        return None
    with open(path, encoding="utf-8") as f:
        return [json.loads(line) for line in f]


def mcp_calls(scenario):
    if not os.path.exists(MCP_LOG):
        return []
    with open(MCP_LOG, encoding="utf-8") as f:
        entries = [json.loads(line) for line in f]
    return [e for e in entries if e.get("scenario") == scenario and isinstance(e.get("body"), dict)]


def prompt_results(events):
    return [e["data"] for e in events if e["kind"] == "prompt_done"]


def received(events, method):
    return [e["data"] for e in events if e["kind"] == "recv" and isinstance(e["data"], dict) and e["data"].get("method") == method]


def message_text(events):
    out = []
    for d in received(events, "session/update"):
        u = d["params"]["update"]
        if u.get("sessionUpdate") == "agent_message_chunk":
            out.append(u["content"].get("text", ""))
    return "".join(out)


def error_text(result):
    return json.dumps(result.get("error") or {}, ensure_ascii=False)


def called(scenario, tool):
    return any(e["body"].get("method") == "tools/call" and e["body"].get("params", {}).get("name") == tool for e in mcp_calls(scenario))


def check_init(ev):
    init_ok = any(e["kind"] == "initialize_done" for e in ev)
    session_ok = any(e["kind"] == "session" and e["data"].get("sessionId") for e in ev)
    listed = any(e["body"].get("method") == "tools/list" and e["auth_ok"] for e in mcp_calls("s01_init"))
    return init_ok and session_ok and listed, f"initialize={init_ok} session={session_ok} mcp_tools_list={listed}"


def check_error_contains(needle):
    def check(ev):
        results = prompt_results(ev)
        text = error_text(results[0]) if results else ""
        return needle in text, text[:160]
    return check


def check_tools(ev):
    read = called("s05_tools", "ue_read_probe")
    write = called("s05_tools", "ue_write_probe")
    asked = len(received(ev, "session/request_permission"))
    raw = json.dumps(ev, ensure_ascii=False)
    leaked = "MARKER_AGENTS_MD" in raw or "m0-agents-skill" in raw
    ok = read and write and asked == 0 and not leaked
    return ok, f"read={read} write={write} permission_requests={asked} agents_md_or_skill_leaked={leaked}"


def check_sandbox(ev):
    asked = len(received(ev, "session/request_permission"))
    created = os.path.exists(os.path.join(WORKSPACE, "probe_write.txt"))
    with open(os.path.join(WORKSPACE, "Docs", "Readme.txt"), encoding="utf-8") as f:
        unchanged = f.read() == "Probe doc for read tool.\n"
    read_ok = "Probe doc for read tool" in message_text(ev)
    ok = asked >= 1 and not created and unchanged and read_ok
    return ok, f"permission_requests={asked} file_created={created} readme_unchanged={unchanged} read_ok={read_ok}"


def check_resume(ev):
    text = message_text(ev)
    return "M0_Test" in text, text[:160].replace("\n", " ")


def check_cancel(ev):
    reasons = [(r.get("result") or {}).get("stopReason") for r in prompt_results(ev)]
    return reasons == ["cancelled", "end_turn"], f"stopReasons={reasons}"


def check_gateway(ev):
    reasons = [(r.get("result") or {}).get("stopReason") for r in prompt_results(ev)]
    read = called("s09_gateway", "ue_read_probe")
    return read and reasons == ["end_turn"], f"read={read} stopReasons={reasons}"


def check_parallel(ev):
    done = [e["data"] for e in ev if e["kind"] == "parallel_done"]
    reasons = [(d.get("result") or {}).get("stopReason") for d in done]
    windows = [d.get("window") or {} for d in done]
    complete = len(done) == 2 and reasons == ["end_turn", "end_turn"] and all(w.get("first") is not None for w in windows)
    if not complete:
        return False, f"stopReasons={reasons} windows={windows}"

    # The runtime may push a whole turn as one update at its end, so update windows do not show overlap. Both prompts were
    # sent at the same moment: run one after the other, the later one finishes at least a whole turn after the earlier one;
    # run side by side, both finish within a fraction of a turn of each other.
    elapsed = sorted(d["elapsed"] for d in done)
    parallel = elapsed[1] < elapsed[0] * PARALLEL_FINISH_RATIO
    return True, f"parallel={parallel} elapsed={elapsed} windows={[(w['first'], w['last'], w['chunks']) for w in windows]}"


CHECKS = [
    ("s01_init", "Q1/Q7 initialize, session/new, MCP handshake", check_init),
    ("s02_nokey", "Q4 no key -> explicit missing-key error", check_error_contains("no API key")),
    ("s03_envkey", "Q4 key from environment reaches DeepSeek", check_error_contains("Authentication Fails")),
    ("s04_dotenv", "Q5 workspace .env ignored when launched from DSH_HOME", check_error_contains("no API key"))
    if SPLIT_LAUNCH_DIR else
    ("s04_dotenv", "Q5 launch-directory .env is read", check_error_contains("Authentication Fails")),
    ("s05_tools", "Q3 MCP read/write without permission prompt; AGENTS.md ignored", check_tools),
    ("s06_sandbox", "Q6 read-only sandbox, escalation rejected", check_sandbox),
    ("s07_resume", "Q8 session/resume keeps context", check_resume),
    ("s08_cancel", "session/cancel then continue", check_cancel),
    ("s09_gateway", "llm-pi-ai openai-completions route", check_gateway),
    ("s10_parallel", "M3.7 two sessions prompted at once both finish (parallel= tells whether they overlapped)", check_parallel),
]

failed = 0
for name, title, check in CHECKS:
    events = load(name)
    if events is None:
        print(f"SKIP  {name:<12} {title}")
        continue
    ok, detail = check(events)
    failed += 0 if ok else 1
    print(f"{'PASS' if ok else 'FAIL'}  {name:<12} {title}  [{detail}]")

sys.exit(1 if failed else 0)
