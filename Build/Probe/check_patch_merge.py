"""Checks that the runtime merges a patch shaped like the editor's generated.patch.yml (Shuttlegent.md section 14.5).

Usage: python -I check_patch_merge.py <runtime exe> <plugin patch> <work dir>
Writes a JSON patch with a provider route, a default route, a skill folder and the Shell switch on, runs the runtime
with --dump-config, and checks each shows up in the merged configuration. Exit code 0 when all do.
"""
import json
import os
import subprocess
import sys

EXE, BASE_PATCH, WORK = (os.path.abspath(arg) for arg in sys.argv[1:4])

# Fixture values, chosen to be unique in the dump
ROUTE = "probe-patch-route"
KEY_VARIABLE = "SG_PROBE_PATCH_KEY"
MODEL = "probe-patch-model"
DUMP_TIMEOUT_SECONDS = 120

os.makedirs(WORK, exist_ok=True)
skill_dir = os.path.join(WORK, "skills").replace("\\", "/")
os.makedirs(skill_dir, exist_ok=True)

# Same rows and fields as FShuttlegentGeneratedPatch::Write
rows = [
    {"id": "llm-pi-ai", "config": {"providers": {ROUTE: {"displayName": "Probe route", "apiKeyEnv": KEY_VARIABLE, "api": "openai-completions", "baseURL": "https://gateway.example/v1", "models": [{"id": MODEL, "name": MODEL}]}}}},
    {"id": "acp", "config": {"provider": ROUTE, "model": MODEL}},
    {"id": "skill-filesystem", "config": {"includeDefaultRoots": False, "customSkillDirs": [skill_dir]}},
    {"id": "tool-pwsh", "disabled": False},
]
patch = os.path.join(WORK, "generated.patch.yml")
with open(patch, "w", encoding="utf-8") as f:
    json.dump(rows, f)

result = subprocess.run([EXE, "--profile", "acp", "--patch", BASE_PATCH, "--patch", patch, "--dump-config"], capture_output=True, text=True, encoding="utf-8", timeout=DUMP_TIMEOUT_SECONDS)
dump = result.stdout
lines = dump.splitlines()


def pwsh_enabled():
    for index, line in enumerate(lines):
        if line.strip() == "- id: tool-pwsh":
            block = lines[index:index + 4]
            return any(entry.strip() == "disabled: false" for entry in block)
    return False


checks = [
    ("runtime exit code 0", result.returncode == 0),
    ("provider route", ROUTE + ":" in dump and KEY_VARIABLE in dump),
    ("default route", "provider: " + ROUTE in dump),
    ("skill folder", skill_dir in dump),
    ("Shell switched on", pwsh_enabled()),
]
failed = 0
for name, ok in checks:
    failed += 0 if ok else 1
    print(("PASS  " if ok else "FAIL  ") + name)
if result.returncode != 0:
    print(result.stderr[:2000])
sys.exit(1 if failed else 0)
