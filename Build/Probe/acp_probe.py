"""ACP client probe for Shuttlegent.

Spawns a dsh ACP process, drives one scenario, logs every protocol line.
Secrets must reach the child only through inherit_env; they are never logged.
Usage: python -I acp_probe.py <scenario.json>

scenario keys:
  name: scenario name, sent to MCP servers as the X-Probe-Scenario header
  command: [exe, args...]          process to spawn
  cwd: absolute workspace dir, sent as the ACP session cwd
  process_cwd: optional launch directory of the runtime process (defaults to cwd)
  env: {name: value}               full child environment additions (child starts from a minimal env)
  inherit_env: [names]             names copied from this process env (e.g. SystemRoot, PATH)
  log: path of jsonl log
  mcp: [{name, url, token}]        optional HTTP MCP servers
  resume: sessionId                optional; uses session/resume instead of session/new
  prompts: [text | {text, image}]  prompts sent in order; image is the path of a PNG sent as an ACP image block
  permission: allow|reject         answer for session/request_permission
  cancel_after: seconds            optional; send session/cancel this long after the first prompt
  set_config: [{configId, value}]  optional session/set_config_option calls before prompting
  parallel_prompts: [text]         optional; one new session per text, all prompts sent at once on the same
                                   connection, then every answer collected (logs parallel_done per session)
  timeout: seconds per request
"""
import base64
import json
import os
import subprocess
import sys
import threading
import time
import queue
import urllib.error
import urllib.request

SCENARIO = json.load(open(sys.argv[1], encoding="utf-8"))
LOG = open(SCENARIO["log"], "w", encoding="utf-8")
T0 = time.time()
LOCK = threading.Lock()


def log(kind, payload):
    with LOCK:
        LOG.write(json.dumps({"t": round(time.time() - T0, 3), "kind": kind, "data": payload}, ensure_ascii=False) + "\n")
        LOG.flush()


env = {}
for name in SCENARIO.get("inherit_env", []):
    if name in os.environ:
        env[name] = os.environ[name]
env.update(SCENARIO.get("env", {}))

proc = subprocess.Popen(
    SCENARIO["command"],
    cwd=SCENARIO.get("process_cwd", SCENARIO["cwd"]),
    env=env,
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
)
log("spawn", {"pid": proc.pid, "command": SCENARIO["command"]})

responses = {}
incoming = queue.Queue()
next_id = [0]


def reader():
    for raw in proc.stdout:
        line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
        if not line:
            continue
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            log("stdout_non_json", line)
            continue
        log("recv", msg)
        incoming.put(msg)
    log("stdout_closed", None)
    incoming.put(None)


def err_reader():
    for raw in proc.stderr:
        log("stderr", raw.decode("utf-8", errors="replace").rstrip("\r\n"))


threading.Thread(target=reader, daemon=True).start()
threading.Thread(target=err_reader, daemon=True).start()


def send(msg):
    log("send", msg)
    proc.stdin.write((json.dumps(msg) + "\n").encode("utf-8"))
    proc.stdin.flush()


def prompt_blocks(item):
    """ACP content blocks of one prompt: plain text, or text with a PNG image."""
    if isinstance(item, str):
        return [{"type": "text", "text": item}]
    with open(item["image"], "rb") as image:
        data = base64.b64encode(image.read()).decode("ascii")
    return [{"type": "image", "mimeType": "image/png", "data": data}, {"type": "text", "text": item["text"]}]


def handle_agent_request(msg):
    method = msg.get("method")
    if method == "session/request_permission":
        answer = SCENARIO.get("permission", "reject")
        option = "allow-once" if answer == "allow" else "reject-once"
        send({"jsonrpc": "2.0", "id": msg["id"], "result": {"outcome": {"outcome": "selected", "optionId": option}}})
    else:
        send({"jsonrpc": "2.0", "id": msg["id"], "error": {"code": -32601, "message": f"client does not implement {method}"}})


def request(method, params, timeout=None, on_wait=None):
    next_id[0] += 1
    rid = next_id[0]
    send({"jsonrpc": "2.0", "id": rid, "method": method, "params": params})
    deadline = time.time() + (timeout or SCENARIO.get("timeout", 120))
    while time.time() < deadline:
        if on_wait is not None:
            on_wait()
        try:
            msg = incoming.get(timeout=0.2)
        except queue.Empty:
            continue
        if msg is None:
            return {"error": "process exited"}
        if "method" in msg and "id" in msg:
            handle_agent_request(msg)
        elif msg.get("id") == rid:
            return msg
    log("timeout", {"method": method})
    return {"error": "timeout"}


def run_parallel(mcp):
    """Opens one session per prompt, sends every prompt before reading any answer, and records when each session's updates arrived."""
    sessions = []
    for _ in SCENARIO["parallel_prompts"]:
        sess = request("session/new", {"cwd": SCENARIO["cwd"], "mcpServers": mcp})
        sessions.append(sess.get("result", {}).get("sessionId"))
    log("parallel_sessions", {"sessionIds": sessions})

    pending = {}
    started = time.time()
    for session_id, text in zip(sessions, SCENARIO["parallel_prompts"]):
        next_id[0] += 1
        pending[next_id[0]] = session_id
        send({"jsonrpc": "2.0", "id": next_id[0], "method": "session/prompt", "params": {"sessionId": session_id, "prompt": [{"type": "text", "text": text}]}})

    windows = {sid: {"first": None, "last": None, "chunks": 0} for sid in sessions}
    results = {}
    deadline = time.time() + SCENARIO.get("timeout", 120)
    while pending and time.time() < deadline:
        try:
            msg = incoming.get(timeout=0.2)
        except queue.Empty:
            continue
        if msg is None:
            break
        if "method" in msg and "id" in msg:
            handle_agent_request(msg)
            continue
        if msg.get("method") == "session/update":
            sid = msg["params"].get("sessionId")
            if sid in windows:
                now = round(time.time() - started, 3)
                window = windows[sid]
                window["first"] = now if window["first"] is None else window["first"]
                window["last"] = now
                window["chunks"] += 1
            continue
        if msg.get("id") in pending:
            sid = pending.pop(msg["id"])
            results[sid] = {"elapsed": round(time.time() - started, 3), "result": msg.get("result"), "error": msg.get("error")}
    for sid in sessions:
        log("parallel_done", {"sessionId": sid, "window": windows[sid], **results.get(sid, {"error": "timeout"})})
    return sessions


try:
    init = request("initialize", {
        "protocolVersion": 1,
        "clientCapabilities": {"fs": {"readTextFile": False, "writeTextFile": False}, "terminal": False},
        "clientInfo": {"name": "shuttlegent-m0", "version": "0.0.1"},
    }, timeout=180)
    log("initialize_done", {"elapsed": round(time.time() - T0, 3)})

    mcp = [{
        "type": "http",
        "name": s["name"],
        "url": s["url"],
        "headers": [{"name": "Authorization", "value": "Bearer " + s["token"]}, {"name": "X-Probe-Scenario", "value": SCENARIO["name"]}],
    } for s in SCENARIO.get("mcp", [])]
    if SCENARIO.get("parallel_prompts"):
        for parallel_session in run_parallel(mcp):
            if parallel_session:
                request("session/close", {"sessionId": parallel_session})
        raise SystemExit(0)
    if SCENARIO.get("resume"):
        sess = request("session/resume", {"sessionId": SCENARIO["resume"], "cwd": SCENARIO["cwd"], "mcpServers": mcp})
        session_id = SCENARIO["resume"]
    else:
        sess = request("session/new", {"cwd": SCENARIO["cwd"], "mcpServers": mcp})
        session_id = sess.get("result", {}).get("sessionId")
    log("session", {"sessionId": session_id})

    for item in SCENARIO.get("set_config", []):
        request("session/set_config_option", {"sessionId": session_id, "configId": item["configId"], "value": item["value"]})

    for index, text in enumerate(SCENARIO.get("prompts", [])):
        if not isinstance(text, str):
            log("prompt_image", {"index": index, "image": os.path.basename(text["image"])})
        started = time.time()
        cancel_sent = [False]

        def maybe_cancel():
            delay = SCENARIO.get("cancel_after")
            if delay is not None and index == 0 and not cancel_sent[0] and time.time() - started > delay:
                cancel_sent[0] = True
                send({"jsonrpc": "2.0", "method": "session/cancel", "params": {"sessionId": session_id}})

        result = request("session/prompt", {"sessionId": session_id, "prompt": prompt_blocks(text)}, on_wait=maybe_cancel)
        log("prompt_done", {"index": index, "elapsed": round(time.time() - started, 3), "result": result.get("result"), "error": result.get("error")})

    fork = SCENARIO.get("fork")
    if fork:
        # The session control plugin announces its port to the editor endpoint (here the mock, which logs it)
        control = None
        deadline = time.time() + 15
        while control is None and time.time() < deadline:
            if os.path.exists(fork["mcp_log"]):
                with open(fork["mcp_log"], encoding="utf-8") as f:
                    for line in f:
                        body = json.loads(line).get("body")
                        if isinstance(body, dict) and body.get("method") == "shuttlegent/runtimeControl":
                            control = body["params"]["url"]
            if control is None:
                time.sleep(0.2)
        log("fork_control", {"url": control})
        if control:
            payload = json.dumps({"method": "fork", "params": {"sessionId": session_id, "userMessage": fork["user_message"], "cut": fork["cut"]}}).encode("utf-8")
            req = urllib.request.Request(control, data=payload, method="POST", headers={"Content-Type": "application/json", "Authorization": "Bearer " + fork["token"]})
            try:
                with urllib.request.urlopen(req, timeout=30) as resp:
                    answer = json.loads(resp.read().decode("utf-8"))
            except urllib.error.HTTPError as error:
                answer = json.loads(error.read().decode("utf-8") or "{}")
            log("fork", answer)
            child = (answer.get("result") or {}).get("sessionId")
            if child:
                resumed = request("session/resume", {"sessionId": child, "cwd": SCENARIO["cwd"], "mcpServers": mcp})
                log("fork_session", {"sessionId": child, "error": resumed.get("error")})
                for index, text in enumerate(fork.get("prompts", [])):
                    result = request("session/prompt", {"sessionId": child, "prompt": prompt_blocks(text)})
                    log("fork_prompt_done", {"index": index, "result": result.get("result"), "error": result.get("error")})
                request("session/close", {"sessionId": child})

    mem = subprocess.run(["tasklist", "/FI", f"PID eq {proc.pid}", "/FO", "CSV", "/NH"], capture_output=True, text=True)
    log("memory", mem.stdout.strip())

    if SCENARIO.get("list_sessions", True):
        request("session/list", {"cwd": SCENARIO["cwd"]})
    if session_id:
        request("session/close", {"sessionId": session_id})
finally:
    try:
        proc.stdin.close()
    except OSError:
        pass
    try:
        proc.wait(timeout=15)
    except subprocess.TimeoutExpired:
        proc.kill()
    log("exit", {"code": proc.returncode})
    LOG.close()
