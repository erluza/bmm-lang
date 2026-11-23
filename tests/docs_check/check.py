#!/usr/bin/env python3
import glob, os, re, subprocess, sys, tempfile, hashlib, json

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BMM = os.path.join(ROOT, "bmm")
os.makedirs("/tmp/opencode", exist_ok=True)
expected = json.load(open(os.path.join(ROOT, "tests", "docs_check", "expected.json")))

failures = 0
executed = 0
skipped = 0
unmarked = 0
mds = sorted(glob.glob(os.path.join(ROOT, "docs", "*.md"))) + [
    os.path.join(ROOT, "README.md"), os.path.join(ROOT, "README.es.md")]

for md in mds:
    lines = open(md, encoding="utf-8").read().splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        if line.startswith("```"):
            info = line[3:].strip()
            code = []
            i += 1
            while i < len(lines) and not lines[i].startswith("```"):
                code.append(lines[i])
                i += 1
            if i >= len(lines):
                failures += 1
                print(f"FAIL {os.path.basename(md)}: unterminated block")
                break
            text = "\n".join(code) + "\n"
            if info == "bmm":
                h = hashlib.sha256(text.encode()).hexdigest()
                if h not in expected:
                    failures += 1
                    print(f"FAIL {os.path.basename(md)}: executable block has no expected output")
                else:
                    fd, path = tempfile.mkstemp(suffix=".bmm", dir="/tmp/opencode")
                    os.write(fd, text.encode()); os.close(fd)
                    p = subprocess.run([BMM, path], capture_output=True, text=True, timeout=30)
                    if p.returncode != 0:
                        failures += 1
                        print(f"FAIL {os.path.basename(md)}: {p.stderr.strip()}")
                    elif p.stdout != expected[h]:
                        failures += 1
                        print(f"FAIL {os.path.basename(md)}: output differs")
                    else:
                        executed += 1
                    os.remove(path)
            elif "noexec" in info and ":" in info:
                skipped += 1
            else:
                unmarked += 1
                failures += 1
                print(f"FAIL {os.path.basename(md)}: unmarked code block")
        i += 1

print(f"Executed {executed} executable blocks; {skipped} non-executable blocks marked")
if failures:
    print(f"{failures} doc block failures")
    sys.exit(1)
sys.exit(0)
