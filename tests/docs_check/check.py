#!/usr/bin/env python3
import glob, os, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BMM = os.path.join(ROOT, "bmm")
failures = 0
blocks = 0
mds = sorted(glob.glob(os.path.join(ROOT, "docs", "*.md"))) + [os.path.join(ROOT, "README.md")]
for md in mds:
    text = open(md).read()
    for m in re.finditer(r"```bmm\n(.*?)```", text, re.S):
        blocks += 1
        code = m.group(1)
        fd, path = tempfile.mkstemp(suffix=".bmm", dir="/tmp/opencode")
        os.write(fd, code.encode()); os.close(fd)
        p = subprocess.run([BMM, path], capture_output=True, text=True, timeout=30)
        if p.returncode != 0:
            failures += 1
            print(f"FAIL in {os.path.basename(md)}: {p.stderr.strip()}")
        os.remove(path)
if failures:
    print(f"{failures} doc code blocks failed")
    sys.exit(1)
else:
    print(f"Executed {blocks} code blocks")
    print("All doc code blocks passed")
    sys.exit(0)
