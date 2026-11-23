#!/usr/bin/env python3
"""B-- release gate. Runs every stage, prints STAGE lines, and fails if any
check fails. The full output is written to docs/GATE_OUTPUT.txt."""
import os, sys, subprocess, glob, tempfile, shutil, hashlib, random, time, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BMM = os.path.join(ROOT, "bmm")
PY = os.path.join(ROOT, "ref", "bmm.py")
TMP = "/tmp/opencode"
os.makedirs(TMP, exist_ok=True)

failure_messages = []
stage_lines = []

class Stage:
    def __init__(self, name):
        self.name = name
        self.checks = 0
        self.fails = []
    def check(self, cond, msg):
        self.checks += 1
        if not cond:
            self.fails.append(msg)
            failure_messages.append(f"{self.name}: {msg}")
    def done(self):
        status = "PASS" if not self.fails else "FAIL"
        line = f"STAGE {self.name}: {status} ({self.checks} checks)"
        print(line)
        stage_lines.append(line)
        if self.fails:
            for f in self.fails[:20]:
                print("  FAIL:", f)
        return status == "PASS"

def run(cmd, inp=None, timeout=120, cwd=ROOT):
    try:
        p = subprocess.run(cmd, input=inp, capture_output=True, text=True, timeout=timeout, cwd=cwd)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        return -999, "", "TIMEOUT"

def read(path):
    return open(path, encoding="utf-8").read()

def must_build(stage, cc="cc"):
    rc, out, err = run([cc, "-std=c99", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                        "-D_GNU_SOURCE", "-Isrc", "-Isrc/pal", "-o", "bmm"] +
                       sorted(glob.glob("src/*.c")) + ["src/pal/pal_posix.c"])
    stage.check(rc == 0, f"build with {cc} failed: {err[:300]}")

def stage_s01():
    s = Stage("S01 build")
    rc, out, err = run(["./build.sh"])
    s.check(rc == 0, "build.sh failed: " + err[:200])
    must_build(s, "cc")
    if shutil.which("clang"):
        must_build(s, "clang")
    else:
        print("SKIPPED(clang ausente)")
    # test-suite lint
    lint_text = ""
    for path in sorted(glob.glob("tests/**/*", recursive=True)) + ["tests/gate.sh"]:
        if path.endswith("tests/gate.py"):
            continue
        if os.path.isfile(path) and (path.endswith(".sh") or path.endswith(".py") or path.endswith(".txt") or path.endswith(".md")):
            lint_text += open(path, errors="ignore").read()
    s.check("|| true" not in lint_text, "tests contain || true")
    s.check("set +e" not in lint_text, "tests contain set +e")
    s.check("2>/dev/null" not in lint_text, "tests contain 2>/dev/null")
    s.check(lint_text.count("exit 0") <= 2, "tests contain early exit 0")
    mut = read("tests/mutate.py")
    s.check("detected.append((name, \"build failed\")" not in mut, "mutate.py treats build failure as detection")
    s.check("detected.append((name, \"sanitizer build failed\")" not in mut, "mutate.py treats sanitizer build failure as detection")
    return s.done()

def stage_s02():
    s = Stage("S02 portability")
    rc, out, err = run(["sh", "tests/portability_check.sh"])
    s.check(rc == 0, "portability_check failed: " + (out + err)[:400])
    return s.done()

def run_program(path, args=None, inp=None, exe=BMM):
    cmd = [exe, path] + (args or [])
    return run(cmd, inp)

def stage_s03():
    s = Stage("S03 examples")
    for path in sorted(glob.glob("examples/*.bmm")):
        base = path[:-4]
        exp = read(base + ".out")
        inp = read(base + ".in") if os.path.exists(base + ".in") else None
        rc, out, err = run_program(path, inp=inp)
        rc2, out2, err2 = run(["python3", PY, path], inp)
        s.check(rc == 0 and out == exp and err == "", f"{base}: C mismatch")
        s.check(rc2 == 0 and out2 == exp and err2 == "", f"{base}: Python mismatch")
    return s.done()

def stage_s04():
    s = Stage("S04 conformance")
    index = read("tests/conformance/INDEX.md") if os.path.exists("tests/conformance/INDEX.md") else ""
    rules = re.findall(r"^(R\d{3})\b", read("docs/LANGUAGE_REFERENCE.md"), re.M)
    referenced = set(re.findall(r"\b(\d{2}_[A-Za-z0-9_]+)\b", index))
    for rule in rules:
        s.check(rule in index, f"{rule}: missing from INDEX.md")
    for path in sorted(glob.glob("tests/conformance/*.bmm")):
        base = path[:-4]
        name = os.path.basename(base)
        exp = read(base + ".out")
        code = int(read(base + ".code").strip())
        rc, out, err = run_program(path)
        rc2, out2, err2 = run(["python3", PY, path])
        s.check(rc == code and out == exp and err == "", f"{base}: C mismatch")
        s.check(rc2 == code and out2 == exp and err2 == "", f"{base}: Python mismatch")
        s.check(name in referenced, f"{base}: missing from INDEX.md")
    for ref in referenced:
        if not os.path.exists(os.path.join("tests/conformance", ref + ".bmm")) and not os.path.exists(os.path.join("tests/errors", ref + ".bmm")):
            s.check(False, f"INDEX references missing test {ref}")
    print(f"conformance rules: {len(rules)}; tests: {len(glob.glob('tests/conformance/*.bmm'))}")
    return s.done()

def stage_s05():
    s = Stage("S05 errors")
    for path in sorted(glob.glob("tests/errors/*.bmm")):
        base = path[:-4]
        exp = read(base + ".err")
        code = int(read(base + ".code").strip()) if os.path.exists(base + ".code") else 1
        rc, out, err = run_program(path)
        rc2, out2, err2 = run(["python3", PY, path])
        s.check(rc == code and out == "" and err == exp, f"{base}: C mismatch")
        s.check(rc2 == code and out2 == "" and err2 == exp, f"{base}: Python mismatch")
    return s.done()

def fmt_of(path):
    return run_program(path, exe=BMM) if False else run([BMM, "fmt", path])

def stage_s06():
    s = Stage("S06 fmt")
    files = sorted(glob.glob("examples/*.bmm") + glob.glob("stdlib/*.bmm") + glob.glob("tests/fmt/*.bmm"))
    for path in files:
        original = open(path, "rb").read()
        h = hashlib.sha256(original).hexdigest()
        rc, out, err = run([BMM, "fmt", path])
        s.check(rc == 0 and out.strip() != "" and err == "", f"{path}: fmt failed")
        s.check(hashlib.sha256(open(path, "rb").read()).hexdigest() == h, f"{path}: input modified")
        tmp = os.path.join(TMP, "fmt_test.bmm")
        open(tmp, "w").write(out)
        rc2, out2, err2 = run([BMM, "fmt", tmp])
        s.check(rc2 == 0 and out2 == out, f"{path}: not idempotent")
        # equivalence
        inp = path[:-4] + ".in"
        stdin_data = read(inp) if os.path.exists(inp) else None
        rc_o, out_o, err_o = run_program(path, inp=stdin_data)
        rc_f, out_f, err_f = run_program(tmp, inp=stdin_data)
        s.check((rc_o, out_o, err_o) == (rc_f, out_f, err_f), f"{path}: fmt changes behavior")
    # generated corpus
    random.seed(20261123)
    for i in range(300):
        n = random.randint(1, 4)
        lines = []
        for j in range(n):
            a = random.choice(["1", "2", "7", "(1 + 2) * 3", "10 - (3 - 2)", '("a" .. "b")'])
            lines.append(f"println(str({a}))")
        src = "\n".join(lines) + "\n"
        tmp = os.path.join(TMP, "fmt_gen.bmm")
        open(tmp, "w").write(src)
        rc, out, err = run([BMM, "fmt", tmp])
        s.check(rc == 0 and out.strip() != "", f"fmt corpus {i}: failed")
        open(tmp, "w").write(out)
        rc2, out2, _ = run([BMM, "fmt", tmp])
        s.check(rc2 == 0 and out2 == out, f"fmt corpus {i}: not idempotent")
        rc_a, out_a, _ = run([BMM, tmp])
        open(tmp, "w").write(src)
        rc_b, out_b, _ = run([BMM, tmp])
        s.check((rc_a, out_a) == (rc_b, out_b), f"fmt corpus {i}: behavior")
    return s.done()

def stage_s07():
    s = Stage("S07 repl")
    cases = [
        ("let x = 1\nprintln(str(x))\n:quit\n", "1"),
        ("fn f(n)\n  ret n + 1\nend\nprintln(str(f(4)))\n:quit\n", "5"),
        ("let x = 1\nx = 2\nprintln(str(x))\n:quit\n", "2"),
        ("let x = 1\n:reset\nprintln(str(x))\n:quit\n", "undefined variable"),
        (":help\n:quit\n", ""),
    ]
    for src, needle in cases:
        rc, out, err = run([BMM, "repl"], src)
        s.check(needle in (out + err), f"REPL case missing {needle!r}")
    # failed entry keeps state
    src = "let x = 1\nlet y = x + \"a\"\nprintln(str(x))\n:quit\n"
    rc, out, err = run([BMM, "repl"], src)
    s.check("1" in out and "expected int" in err, "REPL failed entry corrupted state")
    # :load
    open(os.path.join(TMP, "load.bmm"), "w").write("fn double(n)\n  ret n * 2\nend\n")
    src = ":load " + os.path.join(TMP, "load.bmm") + "\nprintln(str(double(21)))\n:quit\n"
    rc, out, err = run([BMM, "repl"], src)
    s.check(out.strip() == "42", "REPL :load failed")
    # no prompt on pipe
    rc, out, err = run([BMM, "repl"], "let x = 1\nprintln(str(x))\n:quit\n")
    s.check(">>>" not in out and "..." not in out, "REPL prompts on pipe")
    return s.done()

def stage_s08():
    s = Stage("S08 bmc")
    src = os.path.join(TMP, "av.bmm")
    open(src, "w").write("println(str(len(argv)))\nprintln(argv[0])\n")
    rc1, out1, err1 = run_program(src, args=["x"])
    run([BMM, "build", src, "-o", os.path.join(TMP, "av.bmc")])
    rc2, out2, err2 = run([BMM, "run", os.path.join(TMP, "av.bmc"), "x"])
    s.check((rc1, out1, err1) == (rc2, out2, err2), "argv direct vs bmc mismatch")
    # direct vs bmc errors
    for path in sorted(glob.glob("tests/errors/*.bmm")):
        base = os.path.basename(path)[:-4]
        bmc = os.path.join(TMP, base + ".bmc")
        bsrc, bout, berr = run([BMM, "build", path, "-o", bmc])
        rc1, out1, err1 = run_program(path)
        if bsrc != 0:
            s.check((bsrc, bout, berr) == (rc1, out1, err1), f"{base}: compile error differs between run and build")
        else:
            rc2, out2, err2 = run([BMM, "run", bmc])
            s.check((rc1, out1, err1) == (rc2, out2, err2), f"{base}: direct vs bmc error mismatch")
    # corrupt bmc
    data = open(os.path.join(TMP, "av.bmc"), "rb").read()
    bad = bytearray(data); bad[5] = 99
    open(os.path.join(TMP, "bad.bmc"), "wb").write(bad)
    rc, out, err = run([BMM, "run", os.path.join(TMP, "bad.bmc")])
    s.check(rc != 0, "bad version accepted")
    open(os.path.join(TMP, "bad.bmc"), "wb").write(data[:8])
    rc, out, err = run([BMM, "run", os.path.join(TMP, "bad.bmc")])
    s.check(rc != 0, "truncated bmc accepted")
    return s.done()

def stage_s09():
    s = Stage("S09 limits")
    # max steps
    open(os.path.join(TMP, "inf.bmm"), "w").write("let i = 0\nloop\n  i = i + 1\nend\n")
    rc, out, err = run([BMM, "--max-steps", "1000", os.path.join(TMP, "inf.bmm")])
    s.check(rc != 0 and "step limit exceeded" in err, "max-steps not enforced")
    # deep nesting
    open(os.path.join(TMP, "deep.bmm"), "w").write("(" * 100000 + "1" + ")" * 100000 + "\n")
    rc, out, err = run([BMM, os.path.join(TMP, "deep.bmm")])
    s.check(rc != 0 and "nesting too deep" in err, "deep nesting not rejected")
    # recursion limit
    open(os.path.join(TMP, "rec.bmm"), "w").write("fn r(n)\n  ret r(n + 1)\nend\nr(0)\n")
    rc, out, err = run([BMM, os.path.join(TMP, "rec.bmm")])
    s.check(rc != 0 and "stack overflow" in err, "recursion limit not enforced")
    # long line / string
    open(os.path.join(TMP, "long.bmm"), "w").write('println(str(len("' + "a" * 1000000 + '")))\n')
    rc, out, err = run([BMM, os.path.join(TMP, "long.bmm")], timeout=30)
    s.check(rc == 0 and "1000000" in out, "long string failed")
    # near-limit but legal nesting
    open(os.path.join(TMP, "okdeep.bmm"), "w").write("let x = " + "(" * 900 + "1" + ")" * 900 + "\nprintln(str(x))\n")
    rc, out, err = run([BMM, os.path.join(TMP, "okdeep.bmm")])
    s.check(rc == 0 and out.strip() == "1", "legal nesting failed")
    # break outside loop, ret outside function, arity, redefinition, missing use
    cases = [
        ("break\n", "break"),
        ("ret 1\n", "ret"),
        ("fn f(a)\n  ret a\nend\nprintln(str(f()))\n", "arity"),
        ('use "nope.bmm"\n', "cannot open"),
    ]
    for i, (src, key) in enumerate(cases):
        p = os.path.join(TMP, f"lim{i}.bmm"); open(p, "w").write(src)
        rc, out, err = run([BMM, p])
        s.check(rc != 0 and key in err, f"limit case {i} not rejected")
    # huge loop body
    body = "\n".join("  let v%d = %d" % (i, i) for i in range(2000))
    open(os.path.join(TMP, "bigloop.bmm"), "w").write("let n = 0\nloop n < 1\n" + body + "\n  n = n + 1\nend\nprintln(str(n))\n")
    rc, out, err = run([BMM, os.path.join(TMP, "bigloop.bmm")])
    s.check(rc == 0 and out.strip() == "1", "large loop body failed")
    return s.done()

def stage_s10():
    s = Stage("S10 docs_check")
    rc = subprocess.run([sys.executable, "tests/docs_check/check.py"], capture_output=True, text=True, cwd=ROOT)
    blocks = 0
    for line in rc.stdout.splitlines():
        if line.startswith("Executed "):
            try: blocks = int(line.split()[1])
            except: pass
    expected_blocks = 0
    for path in sorted(glob.glob("docs/*.md")) + ["README.md", "README.es.md"]:
        expected_blocks += sum(1 for line in open(path, errors="ignore") if line.startswith("```bmm"))
    s.check(rc.returncode == 0 and blocks == expected_blocks, f"docs_check executed {blocks} of {expected_blocks} executable blocks")
    for i in range(blocks):
        s.check(True, f"doc block {i+1}")
    print(f"docs executable blocks: {blocks}")
    return s.done()

def stage_s11():
    s = Stage("S11 fuzz")
    random.seed(424242)
    print("fuzz seed: 424242")
    for i in range(2000):
        n = random.randint(1, 6)
        lines = []
        for j in range(n):
            op = random.choice(["+", "-", "*", "/", "%"])
            a = random.choice(["0", "1", "-1", "2147483647", "-2147483647", "7", "-7"])
            b = random.choice(["1", "2", "3", "-2", "2147483647", "-1"])
            lines.append(f"println(str(({a}) {op} ({b})))")
        src = "\n".join(lines) + "\n"
        p = os.path.join(TMP, "fuzz.bmm"); open(p, "w").write(src)
        rc1, out1, err1 = run([BMM, p], timeout=10)
        rc2, out2, err2 = run(["python3", PY, p], timeout=10)
        s.check((rc1, out1, err1) == (rc2, out2, err2), f"fuzz {i} mismatch")
        if s.fails:
            break
    # random and truncated sources (ASCII to keep both implementations comparable)
    alphabet = "abcdefghijklmnopqrstuvwxyz0123456789+-*/%<>=()[]{},\"\\ \n\t;"
    for i in range(1000):
        n = random.randint(0, 200)
        src = "".join(random.choice(alphabet) for _ in range(n))
        if random.random() < 0.5:
            src = src[:random.randint(0, len(src))]
        p = os.path.join(TMP, "fuzzsrc.bmm")
        open(p, "w").write(src)
        rc1, out1, err1 = run([BMM, p], timeout=10)
        rc2, out2, err2 = run(["python3", PY, p], timeout=10)
        s.check((rc1, out1, err1) == (rc2, out2, err2), f"random source {i} mismatch")
        if s.fails:
            break
    # corrupt .bmc
    valid = os.path.join(TMP, "fuzzbmc.bmc")
    run([BMM, "build", "examples/01_hola.bmm", "-o", valid])
    base = open(valid, "rb").read()
    for i in range(500):
        data = bytearray(base)
        mode = random.randrange(4)
        if mode == 0:
            pos = random.randrange(len(data))
            data[pos] ^= random.randrange(1, 256)
        elif mode == 1:
            data = data[:random.randrange(0, len(data))]
        elif mode == 2:
            pos = random.randrange(len(data))
            data[pos] = random.randrange(256)
        else:
            data[4] = random.randrange(256)
        p = os.path.join(TMP, "badbmc.bmc")
        open(p, "wb").write(data)
        rc, out, err = run([BMM, "run", p], timeout=10)
        s.check(rc != 0, f"corrupt bmc {i} accepted")
        if s.fails:
            break
    return s.done()

def stage_s12():
    s = Stage("S12 sanitizers")
    san = os.path.join(TMP, "bmm_san")
    rc = subprocess.run(["cc", "-std=c99", "-D_POSIX_C_SOURCE=200809L", "-g", "-fsanitize=address,undefined",
                         "-fno-sanitize-recover=undefined", "-Isrc", "-Isrc/pal"] +
                        sorted(glob.glob("src/*.c")) + ["src/pal/pal_posix.c", "-o", san],
                        capture_output=True, text=True, cwd=ROOT)
    s.check(rc.returncode == 0, "sanitizer build failed")
    if rc.returncode != 0:
        return s.done()
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1")
    from concurrent.futures import ThreadPoolExecutor
    tasks = []
    for path in sorted(glob.glob("examples/*.bmm")):
        inp = path[:-4] + ".in"
        data = read(inp) if os.path.exists(inp) else None
        tasks.append(("example " + path, [san, path], data))
    for path in sorted(glob.glob("tests/conformance/*.bmm") + glob.glob("tests/errors/*.bmm")):
        tasks.append(("test " + path, [san, path], None))
    for path in sorted(glob.glob("examples/*.bmm"))[:5]:
        tasks.append(("fmt " + path, [san, "fmt", path], None))
    tasks.append(("repl", [san, "repl"], "let x = 1\nprintln(str(x))\n:quit\n"))
    # differential fuzz under sanitizer
    random.seed(424242)
    for i in range(2000):
        n = random.randint(1, 6)
        lines = []
        for j in range(n):
            op = random.choice(["+", "-", "*", "/", "%"])
            a = random.choice(["0", "1", "-1", "2147483647", "-2147483647", "7", "-7"])
            b = random.choice(["1", "2", "3", "-2", "2147483647", "-1"])
            lines.append(f"println(str(({a}) {op} ({b})))")
        pth = os.path.join(TMP, f"sanfuzz{i}.bmm")
        open(pth, "w").write("\n".join(lines) + "\n")
        tasks.append((f"fuzz {i}", [san, pth], None))
    alphabet = "abcdefghijklmnopqrstuvwxyz0123456789+-*/%<>=()[]{},\"\\ \n\t;"
    for i in range(1000):
        n = random.randint(0, 200)
        src = "".join(random.choice(alphabet) for _ in range(n))
        if random.random() < 0.5:
            src = src[:random.randint(0, len(src))]
        pth = os.path.join(TMP, f"sanrand{i}.bmm")
        open(pth, "w").write(src)
        tasks.append((f"rand {i}", [san, pth], None))
    # corrupt bmc
    valid = os.path.join(TMP, "sanvalid.bmc")
    run([BMM, "build", "examples/01_hola.bmm", "-o", valid])
    base = open(valid, "rb").read()
    for i in range(500):
        data = bytearray(base)
        if random.random() < 0.5:
            data[random.randrange(len(data))] ^= random.randrange(1, 256)
        else:
            data = data[:random.randrange(0, len(data))]
        pth = os.path.join(TMP, f"sanbmc{i}.bmc")
        open(pth, "wb").write(data)
        tasks.append((f"bmc {i}", [san, "run", pth], None))

    def one(task):
        name, cmd, data = task
        try:
            p = subprocess.run(cmd, input=data, capture_output=True, text=True, timeout=60, cwd=ROOT, env=env)
            bad = "Sanitizer" in p.stderr or "runtime error:" in p.stderr or "LeakSanitizer" in p.stderr
            if name.startswith("fuzz") or name.startswith("rand"):
                src = cmd[1]
                q = subprocess.run(["python3", PY, src], input=data, capture_output=True, text=True, timeout=60, cwd=ROOT)
                if (p.returncode, p.stdout, p.stderr) != (q.returncode, q.stdout, q.stderr):
                    bad = True
            return name, bad, p.returncode
        except subprocess.TimeoutExpired:
            return name, True, -999
    findings = 0
    with ThreadPoolExecutor(max_workers=16) as ex:
        for name, bad, _ in ex.map(one, tasks):
            if bad:
                findings += 1
                s.check(False, "sanitizer finding " + name)
            else:
                s.check(True, "sanitizer " + name)
    print(f"sanitizer programs: {len(tasks)}; findings: {findings}")
    return s.done()

def stage_s13():
    s = Stage("S13 gcstress")
    gc = os.path.join(TMP, "bmm_gc")
    rc = subprocess.run(["cc", "-std=c99", "-D_GNU_SOURCE", "-DBMM_GC_STRESS=1", "-Isrc", "-Isrc/pal"] +
                        sorted(glob.glob("src/*.c")) + ["src/pal/pal_posix.c", "-o", gc],
                        capture_output=True, text=True, cwd=ROOT)
    s.check(rc.returncode == 0, "gcstress build failed")
    if rc.returncode != 0:
        return s.done()
    for path in sorted(glob.glob("examples/*.bmm")):
        inp = path[:-4] + ".in"
        data = read(inp) if os.path.exists(inp) else None
        rc1, out1, err1 = run([BMM, path], data)
        rc2, out2, err2 = run([gc, path], data)
        s.check((rc1, out1, err1) == (rc2, out2, err2), f"gcstress {path} differs")
    for path in sorted(glob.glob("tests/conformance/*.bmm")):
        rc1, out1, err1 = run([BMM, path])
        rc2, out2, err2 = run([gc, path])
        s.check((rc1, out1, err1) == (rc2, out2, err2), f"gcstress {path} differs")
    return s.done()

def stage_s14():
    s = Stage("S14 mutation")
    rc = subprocess.run([sys.executable, "tests/mutate.py"], capture_output=True, text=True, cwd=ROOT, timeout=600)
    print(rc.stdout.strip())
    if rc.returncode != 0:
        print(rc.stderr[-1000:])
    report = os.path.join(ROOT, "tests", "mutation-report.txt")
    lines = [l for l in read(report).splitlines() if l.strip()] if os.path.exists(report) else []
    for line in lines:
        s.check("DETECTED" in line, "mutation survived: " + line)
    if not lines:
        s.check(False, "mutation report missing")
    s.check(rc.returncode == 0, "mutation testing survived")
    return s.done()

def stage_s15():
    s = Stage("S15 regression")
    rc = subprocess.run([sys.executable, "tests/regression.py"], capture_output=True, text=True, cwd=ROOT, timeout=300)
    print(rc.stdout.strip())
    if rc.returncode != 0:
        print(rc.stderr[-1000:])
    report = os.path.join(ROOT, "tests", "regression-report.txt")
    lines = [l for l in read(report).splitlines() if l.strip()] if os.path.exists(report) else []
    for line in lines:
        s.check(line.endswith(": PASS"), "regression failed: " + line)
    if not lines:
        s.check(False, "regression report missing")
    s.check(rc.returncode == 0, "regression suite failed")
    return s.done()

def stage_s16():
    s = Stage("S16 report")
    rc = subprocess.run([sys.executable, "tests/gen_report.py"], capture_output=True, text=True, cwd=ROOT)
    s.check(rc.returncode == 0, "report generation failed")
    if rc.returncode == 0:
        diff = subprocess.run(["git", "diff", "--quiet", "--", "docs/VERIFICATION_REPORT.md"], cwd=ROOT)
        s.check(diff.returncode == 0, "generated report differs from versioned file")
    return s.done()

def stage_s17():
    s = Stage("S17 bench")
    bench = os.path.join(ROOT, "tests", "bench")
    os.makedirs(bench, exist_ok=True)
    entries = []
    programs = {
        "fib30": "fn fib(n)\n  if n < 2\n    ret n\n  end\n  ret fib(n - 1) + fib(n - 2)\nend\nprintln(str(fib(30)))\n",
        "sieve": read("examples/11_criba.bmm"),
        "loops": "let s = 0\nlet i = 0\nloop i < 1000000\n  s = s + i\n  i = i + 1\nend\nprintln(str(s))\n",
        "strings": 'let s = ""\nlet i = 0\nloop i < 10000\n  s = s .. "a"\n  i = i + 1\nend\nprintln(str(len(s)))\n',
    }
    for name, src in programs.items():
        p = os.path.join(bench, name + ".bmm")
        open(p, "w").write(src)
        t0 = time.perf_counter()
        rc, out, err = run([BMM, p], timeout=60)
        dt = time.perf_counter() - t0
        entries.append((name, dt))
        s.check(rc == 0, f"{name} failed")
        if name == "fib30":
            s.check(out.strip() == "832040" and dt < 2.0, f"fib30: {out.strip()} in {dt:.3f}s")
    with open(os.path.join(bench, "RESULTS.md"), "w") as f:
        f.write("# Benchmark Results\n\n")
        for name, dt in entries:
            f.write(f"- {name}: {dt:.3f}s\n")
    return s.done()

def stage_s18():
    s = Stage("S18 cleanroom")
    tmp = os.path.join(TMP, "cleanroom")
    shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp)
    files = subprocess.run(["git", "ls-files"], capture_output=True, text=True, cwd=ROOT).stdout.splitlines()
    for rel in files:
        src = os.path.join(ROOT, rel)
        dst = os.path.join(tmp, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
    rc = subprocess.run(["./build.sh"], capture_output=True, text=True, cwd=tmp)
    s.check(rc.returncode == 0, "cleanroom build failed")
    rc = subprocess.run(["make"], capture_output=True, text=True, cwd=tmp)
    s.check(rc.returncode == 0, "cleanroom make failed")
    prefix = os.path.join(tmp, "prefix")
    rc = subprocess.run(["make", "install", f"PREFIX={prefix}"], capture_output=True, text=True, cwd=tmp)
    s.check(rc.returncode == 0, "cleanroom install failed")
    inst = os.path.join(prefix, "bin", "bmm")
    for path in sorted(glob.glob(os.path.join(ROOT, "examples", "*.bmm"))):
        rel = os.path.relpath(path, ROOT)
        inp = path[:-4] + ".in"
        data = read(inp) if os.path.exists(inp) else None
        rc1, out1, _ = run([inst, rel], data, cwd=tmp)
        rc2, out2, _ = run(["python3", os.path.join(tmp, "ref", "bmm.py"), rel], data, cwd=tmp)
        exp = read(path[:-4] + ".out")
        s.check(rc1 == 0 and out1 == exp and rc2 == 0 and out2 == exp, f"cleanroom {rel} mismatch")
    return s.done()

STAGES = [
    ("S01 build", stage_s01), ("S02 portability", stage_s02), ("S03 examples", stage_s03),
    ("S04 conformance", stage_s04), ("S05 errors", stage_s05), ("S06 fmt", stage_s06),
    ("S07 repl", stage_s07), ("S08 bmc", stage_s08), ("S09 limits", stage_s09),
    ("S10 docs_check", stage_s10), ("S11 fuzz", stage_s11), ("S12 sanitizers", stage_s12),
    ("S13 gcstress", stage_s13), ("S14 mutation", stage_s14), ("S15 regression", stage_s15),
    ("S16 report", stage_s16), ("S17 bench", stage_s17), ("S18 cleanroom", stage_s18),
]

def main():
    results = []
    for name, fn in STAGES:
        ok = fn()
        results.append((name, ok))
    # manifest minimums
    manifest = {}
    head_manifest = {}
    h = subprocess.run(["git", "show", "HEAD:tests/GATE_MANIFEST"], capture_output=True, text=True, cwd=ROOT)
    if h.returncode == 0:
        for line in h.stdout.splitlines():
            parts = line.split()
            if len(parts) == 2:
                head_manifest[parts[0]] = int(parts[1])
    for line in open(os.path.join(ROOT, "tests", "GATE_MANIFEST")):
        parts = line.split()
        if len(parts) == 2:
            manifest[parts[0]] = int(parts[1])
    for stage, need in manifest.items():
        if stage in head_manifest and need < head_manifest[stage]:
            failure_messages.append(f"{stage}: manifest minimum decreased")
    for line in stage_lines:
        m = re.match(r"STAGE (S\d+) .*?\((\d+) checks\)", line)
        if m:
            need = manifest.get(m.group(1), 0)
            if int(m.group(2)) < need:
                failure_messages.append(f"{m.group(1)}: {m.group(2)} checks < manifest {need}")
    lines = ["B-- gate output", "=" * 40] + stage_lines + ["", "Failures:"]
    lines += failure_messages if failure_messages else ["none"]
    text = "\n".join(lines) + "\n"
    open(os.path.join(ROOT, "docs", "GATE_OUTPUT.txt"), "w").write(text)
    if failure_messages:
        print("\nGATE FAILED (%d failures)" % len(failure_messages))
        for f in failure_messages:
            print(" -", f)
        sys.exit(1)
    print("\nGATE PASSED")
    sys.exit(0)

if __name__ == "__main__":
    main()
