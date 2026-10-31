"""Scores a RunEval.ps1 run: which tools each task called, with which arguments, against tasks.json.

Usage: python -I score.py <out_dir>
Prints one line per task, writes report.md and summary.json to out_dir. Exit code 0 when every task passes.
"""
import json
import os
import sys

OUT = sys.argv[1]
HERE = os.path.dirname(os.path.abspath(__file__))

with open(os.path.join(HERE, "tasks.json"), encoding="utf-8") as f:
    TASKS = json.load(f)["tasks"]
with open(os.path.join(HERE, "tools.snapshot.json"), encoding="utf-8") as f:
    WRITES = {t["name"] for t in json.load(f)["tools"] if not t["readOnly"]}

# call_toolset_tool reads or writes by its target tool, with the plugin's own rules (Config/DefaultShuttlegent.ini,
# FShuttlegentToolsets.cpp IsReadOnly): write list, then read list, then whole-word read prefixes; anything else writes
TOOLSET_RULES = {"WriteTools": [], "ReadOnlyTools": [], "ReadOnlyToolPrefixes": []}
with open(os.path.join(HERE, "..", "..", "Config", "DefaultShuttlegent.ini"), encoding="utf-8") as f:
    for line in f:
        key, _, value = line.strip().lstrip("+").partition("=")
        if key in TOOLSET_RULES and value.strip():
            TOOLSET_RULES[key].append(value.strip())


def starts_with_word(name, word):
    if not name.lower().startswith(word.lower()):
        return False
    return len(name) == len(word) or name[len(word)] == "_" or name[len(word)].isupper()


def is_write(call):
    if call["tool"] != "call_toolset_tool":
        return call["tool"] in WRITES
    target = str(call["arguments"].get("tool", ""))
    if any(target.lower() == t.lower() for t in TOOLSET_RULES["WriteTools"]):
        return True
    if any(target.lower() == t.lower() for t in TOOLSET_RULES["ReadOnlyTools"]):
        return False
    return not any(starts_with_word(target, p) for p in TOOLSET_RULES["ReadOnlyToolPrefixes"])

CALLS = []
mock_log = os.path.join(OUT, "eval_mock.log")
if os.path.exists(mock_log):
    with open(mock_log, encoding="utf-8") as f:
        CALLS = [json.loads(line) for line in f]


def stop_reason(task_id):
    path = os.path.join(OUT, f"{task_id}.jsonl")
    if not os.path.exists(path):
        return None
    with open(path, encoding="utf-8") as f:
        for line in f:
            event = json.loads(line)
            if event["kind"] == "prompt_done":
                return (event["data"].get("result") or {}).get("stopReason") or "error"
    return "no answer"


def args_text(calls, tool):
    return [json.dumps(c["arguments"], ensure_ascii=False).lower() for c in calls if c["tool"] == tool]


def score(task):
    calls = [c for c in CALLS if c["task"] == task["id"]]
    called = [c["tool"] for c in calls]
    writes = [c["tool"] for c in calls if is_write(c)]
    checks = []
    reason = stop_reason(task["id"])
    checks.append(("finished", reason == "end_turn", str(reason)))
    for tool in task.get("calls", []):
        checks.append((f"calls {tool}", tool in called, ""))
    if task.get("callsAny"):
        checks.append((f"calls one of {'/'.join(task['callsAny'])}", any(t in called for t in task["callsAny"]), ""))
    for tool in task.get("forbid", []):
        checks.append((f"no {tool}", tool not in called, ""))
    for tool, needles in task.get("args", {}).items():
        texts = args_text(calls, tool)
        for needle in needles:
            checks.append((f"{tool} args has {needle}", any(needle.lower() in t for t in texts), ""))
    for tool, needles in task.get("argsAbsent", {}).items():
        texts = args_text(calls, tool)
        for needle in needles:
            checks.append((f"{tool} args lacks {needle}", not any(needle.lower() in t for t in texts), ""))
    if "maxWrites" in task:
        checks.append((f"writes <= {task['maxWrites']}", len(writes) <= task["maxWrites"], str(len(writes))))
    return {"id": task["id"], "prompt": task["prompt"], "called": called, "writes": len(writes),
            "passed": all(ok for _, ok, _ in checks), "checks": [{"name": n, "ok": ok, "detail": d} for n, ok, d in checks]}


# Tasks left out of the run (RunEval.ps1 -Only) have no log and are not scored
results = [score(task) for task in TASKS if stop_reason(task["id"]) is not None]
passed = sum(r["passed"] for r in results)
for r in results:
    failed = [c["name"] for c in r["checks"] if not c["ok"]]
    print(f"{'PASS' if r['passed'] else 'FAIL'}  {r['id']:<24} calls={' > '.join(r['called']) or '-'}" + (f"  failed: {', '.join(failed)}" if failed else ""))
print(f"\n{passed}/{len(results)} tasks passed")

with open(os.path.join(OUT, "summary.json"), "w", encoding="utf-8") as f:
    json.dump({"passed": passed, "total": len(results), "results": results}, f, ensure_ascii=False, indent=2)
with open(os.path.join(OUT, "report.md"), "w", encoding="utf-8") as f:
    f.write(f"# Tool-choice evaluation\n\n{passed}/{len(results)} tasks passed.\n\n| Task | Result | Tools called | Failed checks |\n|---|---|---|---|\n")
    for r in results:
        failed = ", ".join(c["name"] for c in r["checks"] if not c["ok"]) or "-"
        f.write(f"| {r['id']} | {'PASS' if r['passed'] else 'FAIL'} | {' > '.join(r['called']) or '-'} | {failed} |\n")

sys.exit(0 if passed == len(results) else 1)
