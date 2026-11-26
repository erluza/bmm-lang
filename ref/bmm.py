#!/usr/bin/env python3
"""Reference implementation of the B-- language (executable specification)."""
import sys
import time
import os
sys.setrecursionlimit(10000)

START = time.perf_counter()

class BmmError(Exception):
    def __init__(self, msg, loc=None):
        super().__init__(msg)
        self.msg = msg
        self.loc = loc

KEYWORDS = {"let","fn","if","else","loop","break","ret","use","and","or","not","end"}

class Tok:
    __slots__ = ("kind","val","line","col")
    def __init__(self, kind, val, line, col):
        self.kind = kind; self.val = val; self.line = line; self.col = col

def loc_of(t): return (t.line, t.col)

def lex(src, filename):
    toks = []
    i = 0
    n = len(src)
    line = 1
    col = 1
    while i < n:
        c = src[i]
        if c == "\n":
            if toks and toks[-1].kind != "NL":
                toks.append(Tok("NL", "\n", line, col))
            i += 1; line += 1; col = 1
            continue
        if c == "\r":
            if i+1 < n and src[i+1] == "\n":
                if toks and toks[-1].kind != "NL":
                    toks.append(Tok("NL", "\n", line, col))
                i += 2; line += 1; col = 1
                continue
            i += 1; col += 1
            continue
        if c in " \t":
            i += 1; col += 1
            continue
        if c == "-" and i+1 < n and src[i+1] == "-":
            while i < n and src[i] != "\n":
                i += 1; col += 1
            continue
        if c.isalpha() or c == "_":
            start = i; startcol = col
            while i < n and (src[i].isalnum() or src[i] == "_"):
                i += 1; col += 1
            word = src[start:i]
            if word in KEYWORDS:
                toks.append(Tok("KW", word, line, startcol))
            else:
                toks.append(Tok("IDENT", word, line, startcol))
            continue
        if c.isdigit():
            start = i; startcol = col
            while i < n and src[i].isdigit():
                i += 1; col += 1
            toks.append(Tok("NUM", src[start:i], line, startcol))
            continue
        if c == '"':
            startcol = col
            i += 1; col += 1
            buf = []
            while i < n and src[i] != '"':
                if src[i] == "\\":
                    if i+1 >= n:
                        raise BmmError("unterminated string", (filename, line, startcol))
                    e = src[i+1]
                    if e == "n": buf.append("\n")
                    elif e == "t": buf.append("\t")
                    elif e == "r": buf.append("\r")
                    elif e == "\\": buf.append("\\")
                    elif e == '"': buf.append('"')
                    else:
                        raise BmmError("bad escape in string", (filename, line, col+1))
                    i += 2; col += 2
                else:
                    if src[i] == "\n":
                        raise BmmError("unterminated string", (filename, line, startcol))
                    buf.append(src[i])
                    i += 1; col += 1
            if i >= n:
                raise BmmError("unterminated string", (filename, line, startcol))
            i += 1; col += 1
            toks.append(Tok("STR", "".join(buf), line, startcol))
            continue
        two = src[i:i+2]
        if two in ("==","!=","<=",">=",".."):
            toks.append(Tok("OP", two, line, col)); i += 2; col += 2; continue
        if c in "+-*/%<>=":
            toks.append(Tok("OP", c, line, col)); i += 1; col += 1; continue
        if c == ",":
            toks.append(Tok("COMMA", c, line, col)); i += 1; col += 1; continue
        if c == ";":
            toks.append(Tok("SEMI", ";", line, col)); i += 1; col += 1; continue
        if c == "(":
            toks.append(Tok("LPAREN", c, line, col)); i += 1; col += 1; continue
        if c == ")":
            toks.append(Tok("RPAREN", c, line, col)); i += 1; col += 1; continue
        if c == "[":
            toks.append(Tok("LBRACKET", c, line, col)); i += 1; col += 1; continue
        if c == "]":
            toks.append(Tok("RBRACKET", c, line, col)); i += 1; col += 1; continue
        raise BmmError("unexpected character %r" % c, (filename, line, col))
    if toks and toks[-1].kind != "NL":
        toks.append(Tok("NL", "\n", line, col))
    toks.append(Tok("EOF", None, line, col))
    return toks

class Parser:
    def __init__(self, toks, filename, src):
        self.toks = toks
        self.pos = 0
        self.filename = filename
        self.src = src
    def peek(self):
        return self.toks[self.pos]
    def advance(self):
        t = self.toks[self.pos]; self.pos += 1; return t
    def skip_sep(self):
        while self.peek().kind in ("NL","SEMI"):
            self.advance()
    def expect(self, kind, val=None):
        t = self.peek()
        if t.kind != kind or (val is not None and t.val != val):
            raise BmmError("expected %s" % (val if val else kind), (self.filename, t.line, t.col))
        return self.advance()
    def parse_program(self):
        stmts = []
        self.skip_sep()
        while self.peek().kind != "EOF":
            if self.peek().kind == "KW" and self.peek().val in ("end","else"):
                break
            s = self.parse_stmt()
            if s is not None:
                stmts.append(s)
            self.skip_sep()
        return stmts
    def parse_stmt(self):
        t = self.peek()
        if t.kind == "KW":
            if t.val == "let":
                self.advance()
                if self.peek().kind != "IDENT":
                    raise BmmError("expected identifier", (self.filename, self.peek().line, self.peek().col))
                name = self.advance().val
                self.expect("OP", "=")
                e = self.parse_expr()
                return ("let", name, e, t.line, t.col)
            if t.val == "fn":
                self.advance()
                if self.peek().kind != "IDENT":
                    raise BmmError("expected function name", (self.filename, self.peek().line, self.peek().col))
                name = self.advance().val
                self.expect("LPAREN")
                params = []
                if self.peek().kind != "RPAREN":
                    if self.peek().kind != "IDENT":
                        raise BmmError("expected parameter", (self.filename, self.peek().line, self.peek().col))
                    params.append(self.advance().val)
                    while self.peek().kind == "COMMA":
                        self.advance()
                        if self.peek().kind != "IDENT":
                            raise BmmError("expected parameter", (self.filename, self.peek().line, self.peek().col))
                        params.append(self.advance().val)
                self.expect("RPAREN")
                self.skip_sep()
                body = self.parse_program()
                if not (self.peek().kind == "KW" and self.peek().val == "end"):
                    raise BmmError("expected 'end'", (self.filename, self.peek().line, self.peek().col))
                self.advance()
                return ("fn", name, params, body, t.line, t.col)
            if t.val == "if":
                self.advance()
                cond = self.parse_expr()
                self.skip_sep()
                thenb = self.parse_program()
                elseb = None
                if self.peek().kind == "KW" and self.peek().val == "else":
                    self.advance()
                    self.skip_sep()
                    elseb = self.parse_program()
                if not (self.peek().kind == "KW" and self.peek().val == "end"):
                    raise BmmError("expected 'end'", (self.filename, self.peek().line, self.peek().col))
                self.advance()
                return ("if", cond, thenb, elseb, t.line, t.col)
            if t.val == "loop":
                self.advance()
                cond = None
                if not (self.peek().kind in ("NL","SEMI","EOF") or (self.peek().kind=="KW" and self.peek().val=="end")):
                    cond = self.parse_expr()
                self.skip_sep()
                body = self.parse_program()
                if not (self.peek().kind == "KW" and self.peek().val == "end"):
                    raise BmmError("expected 'end'", (self.filename, self.peek().line, self.peek().col))
                self.advance()
                return ("loop", cond, body, t.line, t.col)
            if t.val == "break":
                self.advance()
                return ("break", t.line, t.col)
            if t.val == "ret":
                self.advance()
                if self.peek().kind in ("NL","SEMI","EOF") or (self.peek().kind=="KW" and self.peek().val in ("end","else")):
                    return ("ret", None, t.line, t.col)
                e = self.parse_expr()
                return ("ret", e, t.line, t.col)
            if t.val == "use":
                self.advance()
                if self.peek().kind != "STR":
                    raise BmmError("expected string path", (self.filename, self.peek().line, self.peek().col))
                path = self.advance().val
                return ("use", path, t.line, t.col)
            if t.val in ("end","else"):
                return None
        e = self.parse_expr()
        if self.peek().kind == "OP" and self.peek().val == "=":
            self.advance()
            rhs = self.parse_expr()
            return ("assign", e, rhs, e[-2], e[-1])
        return ("exprstmt", e, e[-2], e[-1])

    def parse_expr(self): return self.parse_or()
    def parse_or(self):
        left = self.parse_and()
        while self.peek().kind == "KW" and self.peek().val == "or":
            t = self.advance()
            right = self.parse_and()
            left = ("bin","or",left,right,t.line,t.col)
        return left
    def parse_and(self):
        left = self.parse_cmp()
        while self.peek().kind == "KW" and self.peek().val == "and":
            t = self.advance()
            right = self.parse_cmp()
            left = ("bin","and",left,right,t.line,t.col)
        return left
    def parse_cmp(self):
        left = self.parse_concat()
        while self.peek().kind == "OP" and self.peek().val in ("==","!=","<",">","<=",">="):
            t = self.advance()
            right = self.parse_concat()
            left = ("bin",t.val,left,right,t.line,t.col)
        return left
    def parse_concat(self):
        left = self.parse_add()
        while self.peek().kind == "OP" and self.peek().val == "..":
            t = self.advance()
            right = self.parse_add()
            left = ("bin","..",left,right,t.line,t.col)
        return left
    def parse_add(self):
        left = self.parse_mul()
        while self.peek().kind == "OP" and self.peek().val in ("+","-"):
            t = self.advance()
            right = self.parse_mul()
            left = ("bin",t.val,left,right,t.line,t.col)
        return left
    def parse_mul(self):
        left = self.parse_unary()
        while self.peek().kind == "OP" and self.peek().val in ("*","/","%"):
            t = self.advance()
            right = self.parse_unary()
            left = ("bin",t.val,left,right,t.line,t.col)
        return left
    def parse_unary(self):
        t = self.peek()
        if t.kind == "OP" and t.val in ("-","+"):
            self.advance()
            operand = self.parse_unary()
            return ("unary",t.val,operand,t.line,t.col)
        if t.kind == "KW" and t.val == "not":
            self.advance()
            operand = self.parse_unary()
            return ("unary","not",operand,t.line,t.col)
        return self.parse_primary()
    def parse_primary(self):
        t = self.peek()
        if t.kind == "NUM":
            self.advance()
            return ("num", int(t.val), t.line, t.col)
        if t.kind == "STR":
            self.advance()
            return ("str", t.val, t.line, t.col)
        if t.kind == "IDENT":
            self.advance()
            if self.peek().kind == "LPAREN":
                self.advance()
                args = []
                if self.peek().kind != "RPAREN":
                    args.append(self.parse_expr())
                    while self.peek().kind == "COMMA":
                        self.advance()
                        args.append(self.parse_expr())
                self.expect("RPAREN")
                return ("call", t.val, args, t.line, t.col)
            return ("name", t.val, t.line, t.col)
        if t.kind == "LBRACKET":
            self.advance()
            elems = []
            if self.peek().kind != "RBRACKET":
                elems.append(self.parse_expr())
                while self.peek().kind == "COMMA":
                    self.advance()
                    if self.peek().kind == "RBRACKET":
                        break
                    elems.append(self.parse_expr())
            self.expect("RBRACKET")
            return ("arr", elems, t.line, t.col)
        if t.kind == "LPAREN":
            self.advance()
            e = self.parse_expr()
            self.expect("RPAREN")
            return e
        # indexing handled as postfix below? We handle index in a loop
        # But primary may be followed by [ ]. Handle here generically:
        raise BmmError("unexpected token %s" % t.kind, (self.filename, t.line, t.col))

# Need postfix indexing. We'll wrap parse_primary calls? Easier: implement parse_postfix after primary.
# Modify parse_unary/primary flow: after parse_primary, loop for [ ].
# We'll patch by making parse_primary return base, then a postfix loop in parse_unary? 
# Instead, create parse_postfix and call it from parse_unary instead of parse_primary.

# We'll monkeypatch: redefine parse_unary to use parse_postfix.
def parse_postfix(self):
    base = self._primary()
    while self.peek().kind == "LBRACKET":
        t = self.advance()
        idx = self.parse_expr()
        self.expect("RBRACKET")
        base = ("index", base, idx, t.line, t.col)
    return base

def _primary(self):
    t = self.peek()
    if t.kind == "NUM":
        self.advance()
        return ("num", int(t.val), t.line, t.col)
    if t.kind == "STR":
        self.advance()
        return ("str", t.val, t.line, t.col)
    if t.kind == "IDENT":
        self.advance()
        if self.peek().kind == "LPAREN":
            self.advance()
            args = []
            if self.peek().kind != "RPAREN":
                args.append(self.parse_expr())
                while self.peek().kind == "COMMA":
                    self.advance()
                    args.append(self.parse_expr())
            self.expect("RPAREN")
            base = ("call", t.val, args, t.line, t.col)
        else:
            base = ("name", t.val, t.line, t.col)
        # postfix indexing can chain
        while self.peek().kind == "LBRACKET":
            tt = self.advance()
            idx = self.parse_expr()
            self.expect("RBRACKET")
            base = ("index", base, idx, tt.line, tt.col)
        return base
    if t.kind == "LBRACKET":
        self.advance()
        elems = []
        if self.peek().kind != "RBRACKET":
            elems.append(self.parse_expr())
            while self.peek().kind == "COMMA":
                self.advance()
                if self.peek().kind == "RBRACKET":
                    break
                elems.append(self.parse_expr())
        self.expect("RBRACKET")
        base = ("arr", elems, t.line, t.col)
        while self.peek().kind == "LBRACKET":
            tt = self.advance()
            idx = self.parse_expr()
            self.expect("RBRACKET")
            base = ("index", base, idx, tt.line, tt.col)
        return base
    if t.kind == "LPAREN":
        self.advance()
        e = self.parse_expr()
        self.expect("RPAREN")
        base = e
        while self.peek().kind == "LBRACKET":
            tt = self.advance()
            idx = self.parse_expr()
            self.expect("RBRACKET")
            base = ("index", base, idx, tt.line, tt.col)
        return base
    raise BmmError("unexpected token %s" % t.kind, (self.filename, t.line, t.col))

# bind
Parser.parse_primary = parse_postfix
Parser._primary = _primary
# parse_unary should call parse_postfix (renamed primary)
orig_unary = Parser.parse_unary
def parse_unary2(self):
    t = self.peek()
    if t.kind == "OP" and t.val in ("-","+"):
        self.advance()
        operand = self.parse_unary()
        return ("unary", t.val, operand, t.line, t.col)
    if t.kind == "KW" and t.val == "not":
        self.advance()
        operand = self.parse_unary()
        return ("unary", "not", operand, t.line, t.col)
    return self.parse_primary()
Parser.parse_unary = parse_unary2

# ----------Interpreter----------

class Env:
    def __init__(self, scopes):
        self.scopes = scopes  # list of dicts, last is current
    def get(self, name, loc):
        for s in reversed(self.scopes):
            if name in s:
                return s[name]
        raise BmmError("undefined variable '%s'" % name, loc)
    def set(self, name, val, loc):
        for s in reversed(self.scopes):
            if name in s:
                s[name] = val
                return
        raise BmmError("undefined variable '%s'" % name, loc)
    def define(self, name, val):
        self.scopes[-1][name] = val
    def push(self):
        self.scopes.append({})
    def pop(self):
        self.scopes.pop()

def truthy(v, loc):
    if isinstance(v, bool):
        return 1 if v else 0  # not used
    if isinstance(v, int):
        return 1 if v != 0 else 0
    raise BmmError("expected int", loc)

def to_str(v, loc):
    if isinstance(v, int) and not isinstance(v, bool):
        return str(v)
    if isinstance(v, str):
        return v
    raise BmmError("expected int or str", loc)

def fmt_val(v):
    if isinstance(v, bool):
        return "1" if v else "0"
    if isinstance(v, int):
        return str(v)
    if isinstance(v, str):
        return v
    if isinstance(v, list):
        return "[" + ", ".join(fmt_val(x) for x in v) + "]"
    return str(v)

MAX_FRAMES = 1000

class Interp:
    def __init__(self, filename, args):
        self.filename = filename
        self.globals = {}
        self.scopes = [self.globals]
        self.env = Env(self.scopes)
        self.functions = {}  # name -> (params, body, closure_scopes)
        self.frames = 0
        self.in_function = False
        self.steps = 0
        self.max_steps = None
        self.argv = list(args)
        # expose argv as global
        self.globals["argv"] = self.argv
        # register builtins as special names? We'll handle in eval call.
    def run(self, stmts):
        self.exec_block(stmts, self.env)
    def exec_block(self, stmts, env):
        env.push()
        try:
            for s in stmts:
                self.exec_stmt(s, env)
        finally:
            env.pop()
    def exec_stmt(self, s, env):
        self.check_steps()
        kind = s[0]
        if kind == "let":
            _, name, e, line, col = s
            loc = (self.filename, line, col)
            val = self.eval(e, env, loc)
            # define in current scope
            env.define(name, val)
        elif kind == "fn":
            _, name, params, body, line, col = s
            loc = (self.filename, line, col)
            if self.in_function:
                raise BmmError("nested functions are not supported", loc)
            closure = list(env.scopes)
            env.define(name, ("fn", name, params, body, closure, loc))
        elif kind == "if":
            _, cond, thenb, elseb, line, col = s
            loc = (self.filename, line, col)
            v = self.eval(cond, env, loc)
            if truthy(v, loc):
                self.exec_block(thenb, env)
            elif elseb is not None:
                self.exec_block(elseb, env)
        elif kind == "loop":
            _, cond, body, line, col = s
            loc = (self.filename, line, col)
            while True:
                if cond is not None:
                    v = self.eval(cond, env, loc)
                    if not truthy(v, loc):
                        break
                # execute body; handle break via exception
                try:
                    self.exec_block(body, env)
                except BreakSignal:
                    break
        elif kind == "break":
            raise BreakSignal()
        elif kind == "ret":
            _, e, line, col = s
            loc = (self.filename, line, col)
            val = None
            if e is not None:
                val = self.eval(e, env, loc)
            raise ReturnSignal(val)
        elif kind == "use":
            _, path, line, col = s
            loc = (self.filename, line, col)
            # resolve path
            base = os.path.dirname(self.filename) if self.filename not in ("<stdin>","<repl>") else os.getcwd()
            candidates = [os.path.join(base, path), os.path.join(os.getcwd(), path)]
            target = None
            for c in candidates:
                if os.path.isfile(c):
                    target = c; break
            if target is None:
                raise BmmError("cannot open file '%s'" % path, loc)
            src = open(target, encoding="utf-8").read()
            toks = lex(src, target)
            prog = Parser(toks, target, src).parse_program()
            # execute in same global env, but filename context changes
            saved = self.filename
            self.filename = target
            try:
                # run statements in current env (global). Use a new interp-like? exec in current env
                self.exec_stmts_in(prog, env)
            finally:
                self.filename = saved
        elif kind == "assign":
            _, target, rhs, line, col = s
            loc = (self.filename, line, col)
            val = self.eval(rhs, env, loc)
            self.assign(target, val, env, loc)
        elif kind == "exprstmt":
            _, e, line, col = s
            loc = (self.filename, line, col)
            self.eval(e, env, loc)
        else:
            raise BmmError("unknown statement", loc if False else None)
    def exec_stmts_in(self, stmts, env):
        # execute list without new scope (for use at top? but use can appear anywhere)
        for s in stmts:
            self.exec_stmt(s, env)
    def assign(self, target, val, env, loc):
        if target[0] == "name":
            _, name, _, _ = target
            env.set(name, val, loc)
        elif target[0] == "index":
            _, objexpr, idxexpr, _, _ = target
            obj = self.eval(objexpr, env, loc)
            idx = self.eval(idxexpr, env, loc)
            if not isinstance(obj, list):
                raise BmmError("index assignment requires array", loc)
            if not isinstance(idx, int) or isinstance(idx, bool):
                raise BmmError("index must be int", loc)
            if idx < 0 or idx >= len(obj):
                raise BmmError("index out of range", loc)
            obj[idx] = val
        else:
            raise BmmError("invalid assignment target", loc)
    def eval(self, e, env, loc):
        self.check_steps()
        kind = e[0]
        if kind == "num":
            return self.wrap(e[1])
        if kind == "str":
            return e[1]
        if kind == "name":
            _, name, line, col = e
            if name == "argv":
                return self.argv
            # builtin value? clock is function handled as call; argv handled.
            return env.get(name, (self.filename, line, col))
        if kind == "arr":
            _, elems, line, col = e
            return [self.eval(x, env, (self.filename,line,col)) for x in elems]
        if kind == "unary":
            _, op, operand, line, col = e
            l = (self.filename, line, col)
            v = self.eval(operand, env, l)
            if op == "-":
                if not isinstance(v, int) or isinstance(v, bool):
                    raise BmmError("expected int", l)
                if v == -2147483648:
                    return -2147483648
                return self.wrap(-v)
            if op == "+":
                if not isinstance(v, int) or isinstance(v, bool):
                    raise BmmError("expected int", l)
                return v
            if op == "not":
                if not isinstance(v, int) or isinstance(v, bool):
                    raise BmmError("expected int", l)
                return 1 if v == 0 else 0
            raise BmmError("unknown unary", l)
        # wrap helper for arithmetic
        if kind == "bin":
            _, op, left, right, line, col = e
            l = (self.filename, line, col)
            if op == "or":
                lv = self.eval(left, env, l)
                if not isinstance(lv, int) or isinstance(lv, bool):
                    raise BmmError("expected int", l)
                if lv != 0:
                    return 1
                rv = self.eval(right, env, l)
                if not isinstance(rv, int) or isinstance(rv, bool):
                    raise BmmError("expected int", l)
                return 1 if rv != 0 else 0
            if op == "and":
                lv = self.eval(left, env, l)
                if not isinstance(lv, int) or isinstance(lv, bool):
                    raise BmmError("expected int", l)
                if lv == 0:
                    return 0
                rv = self.eval(right, env, l)
                if not isinstance(rv, int) or isinstance(rv, bool):
                    raise BmmError("expected int", l)
                return 1 if rv != 0 else 0
            lv = self.eval(left, env, l)
            rv = self.eval(right, env, l)
            if op == "..":
                if not isinstance(lv, str) or not isinstance(rv, str):
                    raise BmmError("expected str", l)
                return lv + rv
            if op in ("+","-","*","/","%"):
                if not isinstance(lv, int) or isinstance(lv, bool) or not isinstance(rv, int) or isinstance(rv, bool):
                    raise BmmError("expected int", l)
                if op == "+":
                    return self.wrap(lv + rv)
                if op == "-":
                    return self.wrap(lv - rv)
                if op == "*":
                    return self.wrap(lv * rv)
                if op == "/":
                    if rv == 0:
                        raise BmmError("division by zero", l)
                    if lv == -2147483648 and rv == -1:
                        return -2147483648
                    q = abs(lv)//abs(rv)
                    if (lv < 0) != (rv < 0):
                        q = -q
                    return self.wrap(q)
                if op == "%":
                    if rv == 0:
                        raise BmmError("division by zero", l)
                    if lv == -2147483648 and rv == -1:
                        return 0
                    r = abs(lv) % abs(rv)
                    if lv < 0:
                        r = -r
                    return self.wrap(r)
            if op in ("==","!="):
                eq = self.value_eq(lv, rv)
                res = 1 if eq else 0
                if op == "!=":
                    res = 0 if eq else 1
                return res
            if op in ("<",">","<=" ,">="):
                if not isinstance(lv, int) or isinstance(lv, bool) or not isinstance(rv, int) or isinstance(rv, bool):
                    raise BmmError("expected int", l)
                if op == "<": return 1 if lv < rv else 0
                if op == ">": return 1 if lv > rv else 0
                if op == "<=": return 1 if lv <= rv else 0
                if op == ">=": return 1 if lv >= rv else 0
            raise BmmError("unknown operator", l)
        if kind == "call":
            _, name, args, line, col = e
            l = (self.filename, line, col)
            if name == "argv":
                raise BmmError("'argv' is not a function", l)
            # builtins
            if name == "print":
                if len(args) != 1:
                    raise BmmError("print expects 1 argument", l)
                v = self.eval(args[0], env, l)
                sys.stdout.write(fmt_val(v))
                sys.stdout.flush()
                return 0
            if name == "println":
                if len(args) != 1:
                    raise BmmError("println expects 1 argument", l)
                v = self.eval(args[0], env, l)
                sys.stdout.write(fmt_val(v) + "\n")
                sys.stdout.flush()
                return 0
            if name == "input":
                if len(args) != 0:
                    raise BmmError("input expects 0 arguments", l)
                try:
                    line_in = sys.stdin.readline()
                except Exception:
                    line_in = ""
                if line_in.endswith("\n"):
                    line_in = line_in[:-1]
                if line_in.endswith("\r"):
                    line_in = line_in[:-1]
                return line_in
            if name == "len":
                if len(args) != 1:
                    raise BmmError("len expects 1 argument", l)
                v = self.eval(args[0], env, l)
                if isinstance(v, str):
                    return len(v.encode("utf-8"))
                if isinstance(v, list):
                    return len(v)
                raise BmmError("len expects str or arr", l)
            if name == "push":
                if len(args) != 2:
                    raise BmmError("push expects 2 arguments", l)
                a = self.eval(args[0], env, l)
                v = self.eval(args[1], env, l)
                if not isinstance(a, list):
                    raise BmmError("push expects array", l)
                a.append(v)
                return 0
            if name == "pop":
                if len(args) != 1:
                    raise BmmError("pop expects 1 argument", l)
                a = self.eval(args[0], env, l)
                if not isinstance(a, list):
                    raise BmmError("pop expects array", l)
                if not a:
                    raise BmmError("pop from empty array", l)
                return a.pop()
            if name == "str":
                if len(args) != 1:
                    raise BmmError("str expects 1 argument", l)
                v = self.eval(args[0], env, l)
                if isinstance(v, bool):
                    raise BmmError("str expects int or str", l)
                if isinstance(v, int):
                    return str(v)
                if isinstance(v, str):
                    return v
                raise BmmError("str expects int or str", l)
            if name == "int":
                if len(args) != 1:
                    raise BmmError("int expects 1 argument", l)
                v = self.eval(args[0], env, l)
                if isinstance(v, bool):
                    raise BmmError("int expects int or str", l)
                if isinstance(v, int):
                    return v
                if isinstance(v, str):
                    s = v
                    # parse decimal
                    ss = s.strip()  # should we strip? C likely strict. Let's not strip? 
                    # Strict: optional leading '-'
                    t2 = ss
                    if t2 == "":
                        raise BmmError("invalid integer", l)
                    neg = False
                    if t2[0] == "-":
                        neg = True; t2 = t2[1:]
                    if t2 == "" or not t2.isdigit():
                        raise BmmError("invalid integer", l)
                    # ensure no extra
                    val = int(t2)
                    if neg: val = -val
                    return self.wrap(val)
                raise BmmError("int expects int or str", l)
            if name == "chr":
                if len(args) != 1:
                    raise BmmError("chr expects 1 argument", l)
                v = self.eval(args[0], env, l)
                if not isinstance(v, int) or isinstance(v, bool):
                    raise BmmError("chr expects int", l)
                # code point -> UTF-8 char
                try:
                    return chr(v)
                except ValueError:
                    raise BmmError("invalid code point", l)
            if name == "ord":
                if len(args) != 1:
                    raise BmmError("ord expects 1 argument", l)
                v = self.eval(args[0], env, l)
                if not isinstance(v, str):
                    raise BmmError("ord expects str", l)
                b = v.encode("utf-8")
                # must be single character (1-4 bytes forming one code point)
                try:
                    ch = b.decode("utf-8")
                except Exception:
                    raise BmmError("invalid utf-8", l)
                if len(ch) != 1:
                    raise BmmError("ord expects single character", l)
                return ord(ch)
            if name == "sub":
                if len(args) != 3:
                    raise BmmError("sub expects 3 arguments", l)
                x = self.eval(args[0], env, l)
                st = self.eval(args[1], env, l)
                ln = self.eval(args[2], env, l)
                if not isinstance(st, int) or isinstance(st, bool) or not isinstance(ln, int) or isinstance(ln, bool):
                    raise BmmError("sub expects int indices", l)
                if isinstance(x, str):
                    b = x.encode("utf-8")
                    if st < 0 or ln < 0 or st + ln > len(b):
                        raise BmmError("index out of range", l)
                    return b[st:st+ln].decode("utf-8")
                if isinstance(x, list):
                    if st < 0 or ln < 0 or st + ln > len(x):
                        raise BmmError("index out of range", l)
                    return list(x[st:st+ln])
                raise BmmError("sub expects str or arr", l)
            if name == "arr":
                if len(args) != 0:
                    raise BmmError("arr expects 0 arguments", l)
                return []
            if name == "exit":
                if len(args) != 1:
                    raise BmmError("exit expects 1 argument", l)
                v = self.eval(args[0], env, l)
                if not isinstance(v, int) or isinstance(v, bool):
                    raise BmmError("exit expects int", l)
                code = v % 256
                if code < 0: code += 256
                raise SystemExit(code)
            if name == "clock":
                if len(args) != 0:
                    raise BmmError("clock expects 0 arguments", l)
                return int((time.perf_counter() - START) * 1000)
            # user function
            # lookup in env
            try:
                fv = env.get(name, l)
            except BmmError:
                # maybe global
                fv = self.globals.get(name)
                if fv is None:
                    raise BmmError("undefined function '%s'" % name, l)
            if not (isinstance(fv, tuple) and fv and fv[0] == "fn"):
                raise BmmError("'%s' is not a function" % name, l)
            _, _, params, body, closure, floc = fv
            if len(params) != len(args):
                raise BmmError("arity mismatch for '%s'" % name, l)
            if self.frames >= MAX_FRAMES:
                raise BmmError("stack overflow", l)
            self.frames += 1
            # eval args
            argvals = [self.eval(a, env, l) for a in args]
            # new env
            new_scopes = [dict(s) for s in closure] + [{}]
            # Actually share closure dicts, not copy, for globals mutation. Use references.
            new_scopes = list(closure) + [{}]
            new_env = Env(new_scopes)
            for p, v in zip(params, argvals):
                new_env.define(p, v)
            self.in_function = True
            try:
                self.exec_block(body, new_env)
            except ReturnSignal as r:
                self.frames -= 1
                return r.value
            finally:
                self.in_function = False
                self.frames -= 1
            return 0
        if kind == "index":
            _, objexpr, idxexpr, line, col = e
            l = (self.filename, line, col)
            obj = self.eval(objexpr, env, l)
            idx = self.eval(idxexpr, env, l)
            if isinstance(obj, str):
                if not isinstance(idx, int) or isinstance(idx, bool):
                    raise BmmError("index must be int", l)
                b = obj.encode("utf-8")
                if idx < 0 or idx >= len(b):
                    raise BmmError("index out of range", l)
                return b[idx:idx+1].decode("utf-8")
            if isinstance(obj, list):
                if not isinstance(idx, int) or isinstance(idx, bool):
                    raise BmmError("index must be int", l)
                if idx < 0 or idx >= len(obj):
                    raise BmmError("index out of range", l)
                return obj[idx]
            raise BmmError("indexing requires str or arr", l)
        raise BmmError("unknown expression", loc)

    def wrap(self, v):
        # 32-bit signed wrap
        v = v & 0xFFFFFFFF
        if v >= 0x80000000:
            v -= 0x100000000
        return v

    def value_eq(self, a, b):
        if isinstance(a, bool) or isinstance(b, bool):
            return False
        if isinstance(a, int) and isinstance(b, int):
            return a == b
        if isinstance(a, str) and isinstance(b, str):
            return a == b
        if isinstance(a, list) and isinstance(b, list):
            return a is b
        return False

    def check_steps(self):
        self.steps += 1
        if self.max_steps is not None and self.steps > self.max_steps:
            raise BmmError("step limit exceeded")

class BreakSignal(Exception): pass
class ReturnSignal(Exception):
    def __init__(self, value): self.value = value

def parse_source(src, filename):
    toks = lex(src, filename)
    return Parser(toks, filename, src).parse_program()

def run_file(filename, args, max_steps=None):
    src = open(filename, encoding="utf-8").read()
    prog = parse_source(src, filename)
    interp = Interp(filename, args)
    if max_steps is not None:
        interp.max_steps = max_steps
    interp.run(prog)

def format_error(err, src_cache):
    msg = err.msg
    loc = err.loc
    if loc is None:
        return "bmm: error: %s" % msg
    f, line, col = loc
    # get source line
    src = src_cache.get(f)
    if src is None:
        try:
            src = open(f, encoding="utf-8").read()
        except Exception:
            src = ""
        src_cache[f] = src
    lines = src.split("\n")
    srcline = lines[line-1] if 1 <= line <= len(lines) else ""
    caret = " " * (col - 1) + "^" if col >= 1 else "^"
    out = "bmm: %s:%d:%d: error: %s\n  %s\n  %s" % (f, line, col, msg, srcline, caret)
    return out

def main():
    global START
    START = time.perf_counter()
    args = sys.argv[1:]
    # parse flags
    max_steps = None
    trace = False
    # simple arg handling
    if not args:
        # REPL
        repl()
        return
    if args[0] in ("--version",):
        print("bmm 17.14")
        return
    if args[0] in ("--help", "-h"):
        print("usage: bmm [file.bmm [args...]] | -e code | repl | ...")
        return
    if args[0] == "-e":
        code = args[1] if len(args) > 1 else ""
        rest = args[2:]
        src_cache = {}
        try:
            prog = parse_source(code, "<stdin>")
            interp = Interp("<stdin>", rest)
            if max_steps: interp.max_steps = max_steps
            interp.run(prog)
        except SystemExit as e:
            sys.exit(e.code if isinstance(e.code, int) else 0)
        except BmmError as e:
            sys.stderr.write(format_error(e, src_cache) + "\n")
            # trace? no frames in ref
            sys.exit(1)
        return
    if args[0] == "repl":
        repl()
        return
    # file
    # flags may appear
    # handle --trace, --max-steps
    rest = list(args)
    filename = None
    i = 0
    files_args = []
    # find filename (first non-flag)
    positional = []
    i = 0
    while i < len(rest):
        if rest[i] == "--trace":
            trace = True; i += 1; continue
        if rest[i] == "--max-steps":
            max_steps = int(rest[i+1]); i += 2; continue
        positional.append(rest[i]); i += 1
    if not positional:
        repl(); return
    filename = positional[0]
    prog_args = positional[1:]
    src_cache = {}
    try:
        run_file(filename, prog_args, max_steps)
    except SystemExit as e:
        sys.exit(e.code if isinstance(e.code, int) else 0)
    except BmmError as e:
        sys.stderr.write(format_error(e, src_cache) + "\n")
        sys.exit(1)

def repl():
    print("B-- REPL. :help :quit :reset :load")
    env_state = {"src": "", "prog": None}
    # We'll accumulate lines
    buf = []
    interp = None
    filename = "<repl>"
    def reset():
        nonlocal interp
        interp = Interp(filename, [])
    reset()
    # We need persistent env across inputs. Interp has globals; but exec per input with same interp.
    # Accumulate multi-line blocks.
    while True:
        try:
            prompt = "... " if buf else ">>> "
            line = input(prompt)
        except EOFError:
            break
        if not buf and line.startswith(":"):
            cmd = line.strip()
            if cmd == ":quit":
                break
            if cmd == ":help":
                print(":quit :reset :load <file> :help")
                continue
            if cmd == ":reset":
                reset()
                buf = []
                print("reset")
                continue
            if cmd.startswith(":load"):
                parts = cmd.split(None,1)
                if len(parts) < 2:
                    print("usage: :load <file>")
                    continue
                try:
                    src = open(parts[1], encoding="utf-8").read()
                    prog = parse_source(src, parts[1])
                    interp.run(prog)
                except Exception as e:
                    if isinstance(e, BmmError):
                        print(format_error(e, {}))
                    else:
                        print("error:", e)
                continue
            print("unknown command")
            continue
        buf.append(line)
        # check if block complete: try parse
        src = "\n".join(buf)
        try:
            toks = lex(src, filename)
            prog = Parser(toks, filename, src).parse_program()
        except BmmError:
            # maybe incomplete; continue
            continue
        # parse succeeded; but need ensure all blocks closed? Parser stops at EOF; if unclosed, parse_program returns without error? 
        # Heuristic: if src has open block keywords count > end count, keep accumulating.
        # Count
        opens = sum(1 for w in ("fn","if","loop") if w in src.split())  # rough
        # better: count keywords
        # We'll just try execute; if parse succeeded and no trailing open, execute.
        # Determine if there are unmatched opens by checking if last non-empty line is 'end'
        stripped = [x.strip() for x in buf if x.strip()]
        if not stripped:
            buf = []
            continue
        # if any open block not closed, keep accumulating
        # count keyword occurrences
        import re
        kw_open = len(re.findall(r'\b(fn|if|loop)\b', src))
        kw_end = len(re.findall(r'\bend\b', src))
        if kw_open > kw_end:
            continue
        # execute
        try:
            interp.run(prog)
        except SystemExit as e:
            sys.exit(e.code if isinstance(e.code, int) else 0)
        except BmmError as e:
            print(format_error(e, {}))
        except Exception as e:
            print("error:", e)
        buf = []

if __name__ == "__main__":
    main()
