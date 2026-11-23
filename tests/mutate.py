#!/usr/bin/env python3
"""Apply each mutation to a temporary copy and require the probe to detect it."""
import os, sys, subprocess, shutil, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = "/tmp/opencode"
os.makedirs(TMP, exist_ok=True)

def run(cmd, inp=None, timeout=120, cwd=None):
    try:
        p = subprocess.run(cmd, input=inp, capture_output=True, text=True, timeout=timeout, cwd=cwd)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        return -999, "", "TIMEOUT"

def build(copy, flags=None):
    cmd = ["make", "clean"]
    subprocess.run(cmd, cwd=copy, capture_output=True)
    cmd = ["make"] + (flags or [])
    p = subprocess.run(cmd, cwd=copy, capture_output=True, text=True)
    return p.returncode == 0

def prog(copy, path, args=None, inp=None, timeout=120):
    return run([os.path.join(copy, "bmm"), path] + (args or []), inp, cwd=copy, timeout=timeout)

def py(copy, path, inp=None):
    return run(["python3", os.path.join(copy, "ref", "bmm.py"), path], inp, cwd=copy)

MUTATIONS = []

def mutation(name, file, old, new, probe):
    MUTATIONS.append((name, file, old, new, probe))

# M01 use-after-free
mutation("M01 UAF in run_source", "src/main.c",
         "    return result;\n}\n\nstatic int run_file",
         "    volatile int z = vm->had_error; (void)z;\n    return result;\n}\n\nstatic int run_file",
         "asan_main")

# M02 INT_MIN / -1
mutation("M02 INT_MIN / -1 unguarded", "src/vm.c",
         "if (a == INT32_MIN && b == -1) r = INT32_MIN;", "if (0) r = INT32_MIN;", "int32")

# M03 INT_MIN % -1
mutation("M03 INT_MIN % -1 unguarded", "src/vm.c",
         "if (a == INT32_MIN && b == -1) r = 0;", "if (0) r = 0;", "int32")

# M04 signed arithmetic
mutation("M04 signed arithmetic", "src/vm.c",
         "r = (int32_t)((uint32_t)a + (uint32_t)b);", "r = a + b;", "ubsan")

# M05 division by zero
mutation("M05 division by zero unchecked", "src/vm.c",
         'if (b == 0) { vm_error(vm, "division by zero", line, col); break; }',
         'if (0) { vm_error(vm, "division by zero", line, col); break; }', "divzero")

# M06 fmt quotes
mutation("M06 fmt drops string quotes", "src/fmt.c",
         "out_putc(&out, '\"');\n            for (char *p = t->text",
         "// no quote\n            for (char *p = t->text", "fmtquotes")

# M07 fmt parentheses
mutation("M07 fmt drops parentheses", "src/fmt.c",
         "        if (t->kind == TK_STR) {",
         "        if (t->kind == TK_LPAREN || t->kind == TK_RPAREN) continue;\n        if (t->kind == TK_STR) {", "fmtparens")

# M08 fmt associativity parentheses (drop parens only after 'not'? use same skip)
mutation("M08 fmt drops associativity parens", "src/fmt.c",
         "        if (t->kind == TK_STR) {",
         "        if (t->kind == TK_RPAREN) continue;\n        if (t->kind == TK_STR) {", "fmtassoc")

# M09 fmt comments
mutation("M09 fmt drops comments", "src/fmt.c",
         "        if (t->kind == TK_STR) {",
         "        if (t->kind == TK_COMMENT) continue;\n        if (t->kind == TK_STR) {", "fmtcomments")

# M10 fmt writes input
mutation("M10 fmt overwrites input", "src/fmt.c",
         "    out_flush_stdout(&out);",
         "    pal_write_file(path, out.buf, out.len); out_free(&out);", "fmtwrite")

# M11 REPL globals persistence
mutation("M11 REPL drops globals", "src/repl.c",
         "        ast_free(blk);\n        buflen = 0; buf[0] = '\\0';",
         "        ast_free(blk);\n        for (size_t gi = 0; gi < comp.nglobals; gi++) g_defined[gi] = 0;\n        buflen = 0; buf[0] = '\\0';", "replglobals")

# M12 REPL functions persistence
mutation("M12 REPL drops functions", "src/repl.c",
         "        ast_free(blk);\n        buflen = 0; buf[0] = '\\0';",
         "        ast_free(blk);\n        comp.nfuncs = 0;\n        buflen = 0; buf[0] = '\\0';", "replfuncs")

# M13 REPL prompts on pipe
mutation("M13 REPL prompts on pipe", "src/repl.c",
         "    repl_tty = pal_stdin_isatty();", "    repl_tty = 1;", "replprompt")

# M14 :reset no-op
mutation("M14 :reset no-op", "src/repl.c",
         '            if (strcmp(line, ":reset") == 0) {\n                memset(&comp, 0, sizeof(comp));',
         '            if (strcmp(line, ":reset") == 0) {\n                continue;\n                memset(&comp, 0, sizeof(comp));', "replreset")

# M15 bmc argv
mutation("M15 bmc drops argv", "src/main.c",
         "        if (vm->nglobals > 0) { vm->globals[0] = val_arr((Obj*)aa); vm->global_defined[0] = 1; }",
         "        // argv dropped", "bmcargv")

# M16 checksum
mutation("M16 bmc checksum unchecked", "src/bytecode_io.c",
         "if (stored_crc != calc_crc) return -1;", "if (0) return -1;", "bmccrc")

# M17 version
mutation("M17 bmc version unchecked", "src/bytecode_io.c",
         "if (version != 2) return -1;", "if (0) return -1;", "bmcver")

# M18 index bounds
mutation("M18 index bounds unchecked", "src/vm.c",
         'if (idx < 0 || idx >= (int)a->len) { vm_error(vm, "index out of range", line, col); break; }',
         'if (0) { vm_error(vm, "index out of range", line, col); break; }', "oob")

# M19 jump offset truncation
mutation("M19 jump offset truncated", "src/vm.c",
         "fr->ip += 4;\n                fr->ip += (int)off;",
         "fr->ip += 4;\n                fr->ip += (int16_t)off;", "bigjump")

# M20 stack overflow message
mutation("M20 wrong not-a-function message", "src/compiler.c",
         'const char *post = "\' is not a function";',
         'const char *post = "\' stack overflow";', "argvcall")

# M21 GC roots
mutation("M21 GC misses stack roots", "src/gc.c",
         "for (size_t i = 0; i < vm->sp; i++) mark_value(vm->stack[i]);",
         "for (size_t i = 0; i < 0; i++) mark_value(vm->stack[i]);", "gcstress")

# M22 portability
mutation("M22 libc header outside PAL", "src/out.c",
         '#include "pal/pal.h"', '#include "pal/pal.h"\n#include <stdio.h>', "portability")

# M23 conformance output changed
mutation("M23 conformance .out altered", "tests/conformance/01_arith.out",
         "14\n", "99\n", "conformance")

# M24 reference differs
mutation("M24 reference differs", "ref/bmm.py",
         "        if kind == \"num\":\n            return self.wrap(e[1])",
         "        if kind == \"num\":\n            return self.wrap(e[1]) + 1", "differential")

def probe_int32(copy):
    p = os.path.join(copy, "tests/fmt/overflow_probe.bmm")
    open(p, "w").write("println(str(2147483647 + 1))\nprintln(str(-2147483648 / -1))\nprintln(str(-2147483648 % -1))\n")
    rc, out, err = prog(copy, p)
    return not (rc == 0 and out == "-2147483648\n-2147483648\n")

def probe_divzero(copy):
    p = os.path.join(copy, "tests/errors/div_zero.bmm")
    rc, out, err = prog(copy, p)
    exp = open(os.path.join(copy, "tests/errors/div_zero.err")).read()
    return not (rc == 1 and err == exp)

def probe_oob(copy):
    p = os.path.join(copy, "tests/errors/oob.bmm")
    rc, out, err = prog(copy, p)
    exp = open(os.path.join(copy, "tests/errors/oob.err")).read()
    return not (rc == 1 and err == exp)

def probe_argvcall(copy):
    p = os.path.join(copy, "tests/errors/argv_call.bmm")
    rc, out, err = prog(copy, p)
    return "is not a function" not in err

def probe_fmt(copy, which):
    p = os.path.join(copy, "tests/fmt/corpus.bmm")
    rc, out, err = run([os.path.join(copy, "bmm"), "fmt", p], cwd=copy)
    if rc != 0:
        return True
    if which == "quotes":
        return '"a\\nb' not in out
    if which == "parens":
        return "(1 + 2) * 3" not in out
    if which == "assoc":
        return "10 - (3 - 2)" not in out
    if which == "comments":
        return "-- Program prints" not in out
    if which == "write":
        w = os.path.join(copy, "tests/fmt/write_probe.bmm")
        open(w, "w").write("let   x=1\nprintln( str(x) )\n")
        before = open(w, "rb").read()
        rc, out, err = run([os.path.join(copy, "bmm"), "fmt", w], cwd=copy)
        after = open(w, "rb").read()
        return before != after
    return True

def probe_repl(copy, which):
    if which == "globals":
        rc, out, err = run([os.path.join(copy, "bmm"), "repl"], "let x = 1\nprintln(str(x))\n:quit\n", cwd=copy)
        return out.strip() != "1"
    if which == "funcs":
        rc, out, err = run([os.path.join(copy, "bmm"), "repl"], "fn f(n)\n  ret n + 1\nend\nprintln(str(f(1)))\n:quit\n", cwd=copy)
        return out.strip() != "2"
    if which == "prompt":
        rc, out, err = run([os.path.join(copy, "bmm"), "repl"], "let x = 1\nprintln(str(x))\n:quit\n", cwd=copy)
        return ">>>" in out
    if which == "reset":
        rc, out, err = run([os.path.join(copy, "bmm"), "repl"], "let x = 1\n:reset\nprintln(str(x))\n:quit\n", cwd=copy)
        return "undefined variable" not in err
    return True

def probe_bmc(copy, which):
    src = os.path.join(copy, "tests/fmt/argv_probe.bmm")
    open(src, "w").write("println(argv[0])\n")
    bmc = os.path.join(copy, "tests/fmt/argv_probe.bmc")
    r = subprocess.run([os.path.join(copy, "bmm"), "build", src, "-o", bmc], cwd=copy, capture_output=True, text=True)
    if r.returncode != 0:
        return True
    if which == "argv":
        rc, out, err = run([os.path.join(copy, "bmm"), "run", bmc, "Z"], cwd=copy)
        return out.strip() != "Z"
    if which == "crc":
        data = bytearray(open(bmc, "rb").read())
        data[-1] ^= 0xFF
        open(bmc, "wb").write(data)
        rc, out, err = run([os.path.join(copy, "bmm"), "run", bmc], cwd=copy)
        return rc == 0
    if which == "version":
        data = bytearray(open(bmc, "rb").read())
        data[4] = 99
        open(bmc, "wb").write(data)
        rc, out, err = run([os.path.join(copy, "bmm"), "run", bmc], cwd=copy)
        return rc == 0
    return True

def probe_bigjump(copy):
    body = "\n".join("  let v%d = %d" % (i, i) for i in range(5000))
    p = os.path.join(copy, "tests/fmt/bigloop.bmm")
    open(p, "w").write("let n = 0\nloop n < 1\n" + body + "\n  n = n + 1\nend\nprintln(str(n))\n")
    rc, out, err = prog(copy, p, timeout=60)
    return not (rc == 0 and out.strip() == "1")

def probe_gcstress(copy):
    gc = os.path.join(copy, "bmm_gc")
    r = subprocess.run(["cc", "-std=c99", "-D_GNU_SOURCE", "-DBMM_GC_STRESS=1", "-Isrc", "-Isrc/pal"] +
                       sorted([os.path.join(copy, "src", f) for f in os.listdir(os.path.join(copy, "src")) if f.endswith(".c")]) +
                       [os.path.join(copy, "src", "pal", "pal_posix.c"), "-o", gc], cwd=copy, capture_output=True, text=True)
    if r.returncode != 0:
        return True
    p = os.path.join(copy, "examples/07_arreglos.bmm")
    rc, out, err = run([gc, p], cwd=copy)
    return rc != 0

def probe_portability(copy):
    rc, out, err = run(["sh", os.path.join(copy, "tests/portability_check.sh")], cwd=copy)
    return rc != 0

def probe_conformance(copy):
    rc, out, err = prog(copy, os.path.join(copy, "tests/conformance/01_arith.bmm"))
    exp = open(os.path.join(copy, "tests/conformance/01_arith.out")).read()
    return not (rc == 0 and out == exp)

def probe_differential(copy):
    p = os.path.join(copy, "tests/fmt/ref_probe.bmm")
    open(p, "w").write("println(str(7))\n")
    rc1, out1, err1 = prog(copy, p)
    rc2, out2, err2 = py(copy, p)
    return (rc1, out1, err1) != (rc2, out2, err2)

PROBES = {
    "int32": probe_int32, "divzero": probe_divzero, "oob": probe_oob,
    "argvcall": probe_argvcall, "bigjump": probe_bigjump, "gcstress": probe_gcstress,
    "portability": probe_portability, "conformance": probe_conformance,
    "differential": probe_differential,
}

def main():
    survived = []
    detected = []
    for name, file, old, new, probe in MUTATIONS:
        copy = os.path.join(TMP, "mut")
        shutil.rmtree(copy, ignore_errors=True)
        shutil.copytree(ROOT, copy)
        p = os.path.join(copy, file)
        text = open(p).read()
        if old not in text:
            print(f"{name}: MUTATION-PATTERN-MISSING")
            survived.append(name)
            continue
        open(p, "w").write(text.replace(old, new, 1))
        if probe == "ubsan":
            san = os.path.join(copy, "bmm_ubsan")
            r = subprocess.run(["cc", "-std=c99", "-D_GNU_SOURCE", "-g", "-fsanitize=undefined",
                                "-fno-sanitize-recover=undefined", "-Isrc", "-Isrc/pal"] +
                               sorted([os.path.join(copy, "src", f) for f in os.listdir(os.path.join(copy, "src")) if f.endswith(".c")]) +
                               [os.path.join(copy, "src", "pal", "pal_posix.c"), "-o", san], cwd=copy, capture_output=True, text=True)
            if r.returncode != 0:
                detected.append((name, "ubsan build failed"))
                continue
            ov = os.path.join(copy, "tests/fmt/overflow_probe.bmm")
            open(ov, "w").write("println(str(2147483647 + 1))\n")
            p2 = subprocess.run([san, ov], cwd=copy, capture_output=True, text=True)
            hit = "runtime error:" in p2.stderr
            if hit:
                detected.append((name, "ubsan"))
            else:
                survived.append(name)
            continue
        if "asan" in probe or name.startswith("M01"):
            san = os.path.join(copy, "bmm_san")
            r = subprocess.run(["cc", "-std=c99", "-D_POSIX_C_SOURCE=200809L", "-g", "-fsanitize=address,undefined",
                                "-fno-sanitize-recover=undefined", "-Isrc", "-Isrc/pal"] +
                               sorted([os.path.join(copy, "src", f) for f in os.listdir(os.path.join(copy, "src")) if f.endswith(".c")]) +
                               [os.path.join(copy, "src", "pal", "pal_posix.c"), "-o", san], cwd=copy, capture_output=True, text=True)
            if r.returncode != 0:
                detected.append((name, "sanitizer build failed"))
                continue
            env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1")
            p2 = subprocess.run([san, os.path.join(copy, "examples/01_hola.bmm")], cwd=copy, capture_output=True, text=True, env=env)
            hit = "Sanitizer" in p2.stderr or "runtime error:" in p2.stderr
            if hit:
                detected.append((name, "sanitizer"))
            else:
                survived.append(name)
            continue
        if not build(copy):
            detected.append((name, "build failed"))
            continue
        if probe in PROBES:
            hit = PROBES[probe](copy)
        elif probe.startswith("fmt"):
            hit = probe_fmt(copy, probe[3:])
        elif probe.startswith("repl"):
            hit = probe_repl(copy, probe[4:])
        elif probe.startswith("bmc"):
            hit = probe_bmc(copy, probe[3:])
        else:
            hit = True
        if hit:
            detected.append((name, probe))
        else:
            survived.append(name)
        print(f"{name}: {'DETECTED' if hit else 'SURVIVED'}")
    report = [f"{n}: DETECTED ({p})" for n, p in detected] + [f"{n}: SURVIVED" for n in survived]
    open(os.path.join(ROOT, "tests", "mutation-report.txt"), "w").write("\n".join(report) + "\n")
    print(f"mutations: {len(MUTATIONS)} total, {len(detected)} detected, {len(survived)} survived")
    for n in survived:
        print("  SURVIVED:", n)
    if survived:
        sys.exit(1)
    sys.exit(0)

if __name__ == "__main__":
    main()
