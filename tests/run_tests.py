#!/usr/bin/env python3
import os, subprocess, sys, time, glob, random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BMM = os.path.join(ROOT, "bmm")
PYREF = os.path.join(ROOT, "ref", "bmm.py")

def run(cmd, inp=None, timeout=30):
    try:
        p = subprocess.run(cmd, input=inp, capture_output=True, text=True, timeout=timeout)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        return -999, "", "TIMEOUT"

def compare(a, b):
    return a == b

failures = []
def fail(msg):
    failures.append(msg)
    print("FAIL:", msg)

# Build
print("Building...")
rc = subprocess.run(["make"], cwd=ROOT, capture_output=True, text=True)
if rc.returncode != 0:
    print(rc.stdout); print(rc.stderr); sys.exit(1)

# Examples
print("== Examples ==")
for path in sorted(glob.glob(os.path.join(ROOT, "examples", "*.bmm"))):
    name = os.path.basename(path)
    base = name[:-4]
    outp = path[:-4] + ".out"
    inp_path = path[:-4] + ".in"
    stdin_data = None
    if os.path.exists(inp_path):
        with open(inp_path) as f: stdin_data = f.read()
    with open(outp) as f: expected = f.read()
    # C
    rc1, out1, err1 = run([BMM, path], stdin_data)
    # Python
    rc2, out2, err2 = run(["python3", PYREF, path], stdin_data)
    if rc1 != 0:
        fail(f"example {name}: C exit {rc1}: {err1}")
    if rc2 != 0:
        fail(f"example {name}: PY exit {rc2}: {err2}")
    if out1 != expected:
        fail(f"example {name}: C output mismatch")
    if out2 != expected:
        fail(f"example {name}: PY output mismatch")
    if out1 != out2:
        fail(f"example {name}: C/PY output differ")

# Conformance
print("== Conformance ==")
cdir = os.path.join(ROOT, "tests", "conformance")
for path in sorted(glob.glob(os.path.join(cdir, "*.bmm"))):
    name = os.path.basename(path)
    outp = path[:-4] + ".out"
    if not os.path.exists(outp): 
        continue
    with open(outp) as f: expected = f.read()
    rc1, out1, err1 = run([BMM, path])
    rc2, out2, err2 = run(["python3", PYREF, path])
    if rc1 != 0:
        fail(f"conformance {name}: C exit {rc1}: {err1}")
    if rc2 != 0:
        fail(f"conformance {name}: PY exit {rc2}: {err2}")
    if out1 != expected or out2 != expected or out1 != out2:
        fail(f"conformance {name}: output mismatch")

# Errors
print("== Errors ==")
edir = os.path.join(ROOT, "tests", "errors")
for path in sorted(glob.glob(os.path.join(edir, "*.bmm"))):
    name = os.path.basename(path)
    errp = path[:-4] + ".err"
    if not os.path.exists(errp): 
        continue
    with open(errp) as f: expected_err = f.read()
    rel = os.path.relpath(path, ROOT)
    rc1, out1, err1 = run([BMM, rel])
    rc2, out2, err2 = run(["python3", PYREF, rel])
    if rc1 == 0:
        fail(f"error {name}: C expected failure but succeeded")
    if rc2 == 0:
        fail(f"error {name}: PY expected failure but succeeded")
    if err1 != expected_err:
        fail(f"error {name}: C stderr mismatch")
    if err2 != expected_err:
        fail(f"error {name}: PY stderr mismatch")
    if err1 != err2:
        fail(f"error {name}: C/PY stderr differ")

# Docs check
print("== Docs check ==")
rc = subprocess.run([sys.executable, os.path.join(ROOT, "tests", "docs_check", "check.py")], capture_output=True, text=True)
print(rc.stdout.strip())
if rc.returncode != 0:
    fail("docs_check failed: " + rc.stderr)

# CLI tests
print("== CLI ==")
import tempfile
# fmt idempotence + equivalence for all examples
for ex in sorted(glob.glob(os.path.join(ROOT, "examples", "*.bmm"))):
    name = os.path.basename(ex)
    src = open(ex).read()
    fd, tmp = tempfile.mkstemp(suffix=".bmm", dir="/tmp/opencode")
    os.write(fd, src.encode()); os.close(fd)
    rc, fmt_out, err = run([BMM, "fmt", ex]) if False else (0, None, None)
    # run fmt to stdout by invoking on temp copy
    rc, fmt_out, err = run([BMM, "fmt", tmp])
    if rc != 0 or not fmt_out.strip():
        fail(f"fmt failed on {name}: {err}")
        os.remove(tmp); continue
    open(tmp, "w").write(fmt_out)
    # idempotence
    rc2, fmt_out2, err2 = run([BMM, "fmt", tmp])
    if rc2 != 0 or fmt_out2 != fmt_out:
        fail(f"fmt not idempotent on {name}")
    # equivalence of output
    inp_path = ex[:-4] + ".in"
    stdin_data = open(inp_path).read() if os.path.exists(inp_path) else None
    rc_o, out_o, _ = run([BMM, ex], stdin_data)
    rc_f, out_f, _ = run([BMM, tmp], stdin_data)
    if rc_o != 0 or rc_f != 0 or out_o != out_f:
        fail(f"fmt changes behavior on {name}")
    os.remove(tmp)
# check
rc, out, err = run([BMM, "check", os.path.join(ROOT, "examples", "01_hola.bmm")])
if rc != 0:
    fail("check failed")
# dis
rcb = run([BMM, "build", os.path.join(ROOT, "examples", "01_hola.bmm"), "-o", "/tmp/opencode/hola.bmc"])
rc, out, err = run([BMM, "dis", "/tmp/opencode/hola.bmc"])
if rc != 0:
    fail("dis failed")
# round-trip build->run vs direct
run([BMM, "build", os.path.join(ROOT, "examples", "02_variables.bmm"), "-o", "/tmp/opencode/v.bmc"])
rc1, out1, _ = run([BMM, "run", "/tmp/opencode/v.bmc"])
rc2, out2, _ = run([BMM, os.path.join(ROOT, "examples", "02_variables.bmm")])
if out1 != out2:
    fail("build/run round-trip mismatch")

# REPL smoke
inp = ":help\n:reset\n:load " + os.path.join(ROOT, "examples", "01_hola.bmm") + "\n:quit\n"
rc, out, err = run([BMM], inp)
if rc != 0 or "Hello, world" not in out:
    fail("REPL smoke failed")

# Fuzz
print("== Fuzz ==")
fuzz_dir = os.path.join(ROOT, "tests", "fuzz")
random.seed(12345)
crash = False
for t in range(30):
    # generate a small random program
    lines = []
    n = random.randint(1, 8)
    for _ in range(n):
        kind = random.choice(["let", "print", "if", "loop", "fn"])
        if kind == "let":
            lines.append(f"let v{random.randint(0,3)} = {random.randint(0,5)}")
        elif kind == "print":
            lines.append(f"println(str({random.randint(0,5)}))")
        elif kind == "if":
            lines.append(f"if {random.randint(0,2)} == 0")
            lines.append(f"  println(\"x\")")
            lines.append("end")
        elif kind == "loop":
            lines.append("let i = 0")
            lines.append("loop i < 3")
            lines.append("  i = i + 1")
            lines.append("end")
        elif kind == "fn":
            lines.append("fn f(x)")
            lines.append("  ret x + 1")
            lines.append("end")
            lines.append("println(str(f(1)))")
    src = "\n".join(lines) + "\n"
    tmp = os.path.join(fuzz_dir, "fuzz_tmp.bmm")
    with open(tmp, "w") as f: f.write(src)
    rc1, out1, err1 = run([BMM, tmp], timeout=10)
    rc2, out2, err2 = run(["python3", PYREF, tmp], timeout=10)
    if rc1 == -999 or rc2 == -999:
        fail(f"fuzz {t}: timeout")
        break
    # ignore output; just no crash (segfault would be negative rc)
    if rc1 < 0 and rc1 != 1:
        fail(f"fuzz {t}: C crashed rc={rc1}")
        crash = True
        break
os.system(f"rm -f {os.path.join(fuzz_dir, 'fuzz_tmp.bmm')}")

# BMC corruption fuzz
print("== BMC fuzz ==")
run([BMM, "build", os.path.join(ROOT, "examples", "01_hola.bmm"), "-o", "/tmp/opencode/hola.bmc"])
data = open("/tmp/opencode/hola.bmc","rb").read()
for t in range(20):
    b = bytearray(data)
    # mutate
    import random
    i = random.randrange(len(b))
    b[i] ^= random.randrange(1,256)
    # sometimes truncate
    if random.random() < 0.3:
        b = b[:random.randrange(1,len(b))]
    open("/tmp/opencode/bad.bmc","wb").write(b)
    rc, out, err = run([BMM, "run", "/tmp/opencode/bad.bmc"], timeout=10)
    if rc == 0:
        fail(f"bmc fuzz {t}: accepted corrupt file")
    if rc == -999:
        fail(f"bmc fuzz {t}: timeout")
# wrong version
b = bytearray(data); b[5] = 99; open("/tmp/opencode/bad.bmc","wb").write(b)
rc, out, err = run([BMM, "run", "/tmp/opencode/bad.bmc"])
if rc == 0: fail("bmc wrong version accepted")
# truncated
open("/tmp/opencode/bad.bmc","wb").write(data[:5])
rc, out, err = run([BMM, "run", "/tmp/opencode/bad.bmc"])
if rc == 0: fail("bmc truncated accepted")

# fmt corpus
for cf in sorted(glob.glob(os.path.join(ROOT, "tests", "fmt", "*.bmm"))):
    src = open(cf).read()
    fd, tmp = tempfile.mkstemp(suffix=".bmm", dir="/tmp/opencode")
    os.write(fd, src.encode()); os.close(fd)
    rc, fmt_out, err = run([BMM, "fmt", tmp])
    if rc != 0 or not fmt_out.strip():
        fail(f"fmt corpus failed: {err}"); os.remove(tmp); continue
    open(tmp, "w").write(fmt_out)
    rc2, fmt_out2, _ = run([BMM, "fmt", tmp])
    if rc2 != 0 or fmt_out2 != fmt_out:
        fail("fmt corpus not idempotent")
    os.remove(tmp)

# Sanitizers
print("== Sanitizers ==")
san = "/tmp/bmm_san"
rc = subprocess.run(["cc", "-std=c99", "-D_POSIX_C_SOURCE=200809L", "-g", "-fsanitize=address,undefined",
                     "-fno-sanitize-recover=undefined", "-Isrc", "-Isrc/pal"] +
                    sorted(glob.glob(os.path.join(ROOT, "src", "*.c"))) +
                    [os.path.join(ROOT, "src", "pal", "pal_posix.c")] +
                    ["-o", san], capture_output=True, text=True)
if rc.returncode != 0:
    fail("sanitizer build failed: " + rc.stderr)
else:
    # run a subset: examples + conformance + errors + fmt + repl
    san_fail = False
    for path in sorted(glob.glob(os.path.join(ROOT, "examples", "*.bmm"))):
        inp = path[:-4] + ".in"
        stdin_data = open(inp).read() if os.path.exists(inp) else None
        r = subprocess.run([san, path], input=stdin_data, capture_output=True, text=True, timeout=60)
        if "Sanitizer" in r.stderr or "runtime error:" in r.stderr:
            fail(f"sanitizer finding in {os.path.basename(path)}"); san_fail = True
    for path in sorted(glob.glob(os.path.join(ROOT, "tests", "conformance", "*.bmm"))):
        r = subprocess.run([san, path], capture_output=True, text=True, timeout=60)
        if "Sanitizer" in r.stderr:
            fail(f"sanitizer finding in {os.path.basename(path)}"); san_fail = True
    for path in sorted(glob.glob(os.path.join(ROOT, "tests", "errors", "*.bmm"))):
        r = subprocess.run([san, path], capture_output=True, text=True, timeout=60)
        if "Sanitizer" in r.stderr:
            fail(f"sanitizer finding in {os.path.basename(path)}"); san_fail = True
    # fmt
    for path in sorted(glob.glob(os.path.join(ROOT, "examples", "*.bmm"))):
        r = subprocess.run([san, "fmt", path], capture_output=True, text=True, timeout=60)
        if "Sanitizer" in r.stderr:
            fail(f"sanitizer finding in fmt {os.path.basename(path)}"); san_fail = True
    # repl
    r = subprocess.run([san, "repl"], input="let x = 1\nprintln(str(x))\n:quit\n", capture_output=True, text=True, timeout=60)
    if "Sanitizer" in r.stderr:
        fail("sanitizer finding in REPL")
    if not san_fail:
        print("Sanitizer run: 0 findings")

# Bench
print("== Bench ==")
bench_dir = os.path.join(ROOT, "tests", "bench")
os.makedirs(bench_dir, exist_ok=True)
# fib30
fib_src = os.path.join(bench_dir, "fib30.bmm")
with open(fib_src, "w") as f:
    f.write("fn fib(n)\n  if n < 2\n    ret n\n  end\n  ret fib(n - 1) + fib(n - 2)\nend\nprintln(str(fib(30)))\n")
start = time.perf_counter()
rc, out, err = run([BMM, fib_src], timeout=60)
fib_elapsed = time.perf_counter() - start
print(f"fib(30) C: {out.strip()} in {fib_elapsed:.3f}s")
if fib_elapsed > 2.0:
    fail(f"fib(30) too slow: {fib_elapsed:.3f}s")
# sieve
sieve_src = os.path.join(bench_dir, "sieve.bmm")
with open(sieve_src, "w") as f:
    f.write(open(os.path.join(ROOT, "examples", "11_criba.bmm")).read())
start = time.perf_counter()
rc, out, err = run([BMM, sieve_src], timeout=60)
sieve_elapsed = time.perf_counter() - start
# loops
loops_src = os.path.join(bench_dir, "loops.bmm")
with open(loops_src, "w") as f:
    f.write("let s = 0\nlet i = 0\nloop i < 1000000\n  s = s + i\n  i = i + 1\nend\nprintln(str(s))\n")
start = time.perf_counter()
rc, out, err = run([BMM, loops_src], timeout=60)
loops_elapsed = time.perf_counter() - start
# strings
strings_src = os.path.join(bench_dir, "strings.bmm")
with open(strings_src, "w") as f:
    f.write("let s = \"\"\nlet i = 0\nloop i < 10000\n  s = s .. \"a\"\n  i = i + 1\nend\nprintln(str(len(s)))\n")
start = time.perf_counter()
rc, out, err = run([BMM, strings_src], timeout=60)
strings_elapsed = time.perf_counter() - start
with open(os.path.join(bench_dir, "RESULTS.md"), "w") as f:
    f.write("# Benchmark Results\n\n")
    f.write(f"- fib(30): {fib_elapsed:.3f}s\n")
    f.write(f"- sieve: {sieve_elapsed:.3f}s\n")
    f.write(f"- loops: {loops_elapsed:.3f}s\n")
    f.write(f"- strings: {strings_elapsed:.3f}s\n")

print("\n== Summary ==")
if failures:
    print(f"{len(failures)} failures:")
    for x in failures: print(" -", x)
    sys.exit(1)
else:
    print("All tests passed.")
    sys.exit(0)
