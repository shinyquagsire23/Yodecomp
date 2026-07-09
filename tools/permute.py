#!/usr/bin/env python3
"""Permuter for the Yodecomp matching decomp (VC++ 4.2 under wine).

Given a .cpp and a target function address, it searches semantically-equivalent
source variations and compiles each with cl 4.2, using the relocation-masked
byte-match against the original as the oracle. Stops when a variant byte-matches.

Transformations (the ones proven to move MSVC 4.2 codegen):
  * STATEMENT reordering  — hill-climb over adjacent *independent* statement swaps
    (data-dependency-safe; declaration order is NOT touched, so stack-slot offsets
    stay put — vital for functions whose inline __asm references locals by name).
    This auto-discovers wins like "assign `s = src;` before declaring `rows`" or
    "declare `stride` before `cw`" that were previously found by hand.
  * permute the leading local-declaration block   (-> register / x87-slot allocation)
  * toggle constant comparison form  `x < N` <-> `x <= N-1`, `x > N` <-> `x >= N+1`
  * flip relational operands          `a < b` <-> `b > a`   (cheap; sometimes helps)

Usage:  tools/permute.py src/GameObjects.cpp 0x405330 [--iters 4000] [--mode all|stmt|decl]

The target function is the one whose `// FUNCTION: YODA 0xADDR` marker matches.
Only that function's body is mutated; the rest of the file is kept (TU context).
"""
import sys, os, re, random, subprocess, itertools
import functools
print = functools.partial(print, flush=True)

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match
import asmscore

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CL = os.path.join(ROOT, "toolchain/bin/cl")
FLAGS = "/nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS".split()
EXE = open(os.path.join(ROOT, "YodaDemo/YodaDemo.exe"), "rb").read()
ENV = dict(os.environ, WINEDEBUG="-all")


def extract_func(text, addr):
    """Return (start_idx, end_idx) spanning the function whose marker == addr,
    from the marker line through the matching closing brace."""
    m = re.search(r"//\s*FUNCTION:\s*YODA\s+0x0*%x\b" % addr, text, re.I)
    if not m:
        raise SystemExit("marker for %#x not found" % addr)
    brace = text.index("{", m.end())
    depth = 0
    i = brace
    while i < len(text):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return m.start(), i + 1
        i += 1
    raise SystemExit("unbalanced braces")


TYPE_KW = r"(?:unsigned\s+)?(?:int|short|char|long|float|double|void|bool|Zone|ZoneObj|CFile|GameDoc|World|[A-Z]\w*)"


def split_decls(func_text):
    """Split a function into (head_incl_open_brace, decl_lines[], rest_incl_close).
    Multi-declarator lines (`int y, x;` with no initializer) are split so each name
    can be reordered independently."""
    ob = func_text.index("{")
    head = func_text[:ob + 1]
    lines = func_text[ob + 1:].split("\n")
    decl_re = re.compile(r"^\s*" + TYPE_KW + r"[\s\*]+[A-Za-z_].*;\s*$")
    ctrl = re.compile(r"\b(if|for|while|do|return|switch|new|delete)\b")
    multi = re.compile(r"^(\s*)(" + TYPE_KW + r")\s+([A-Za-z_][\w,\s\*]*);\s*$")
    decls, i = [], 0
    while i < len(lines):
        ln = lines[i]
        if ln.strip() == "":
            i += 1
            continue
        if decl_re.match(ln) and not ctrl.search(ln) and "(" not in ln:
            mm = multi.match(ln)
            if mm and "," in mm.group(3) and "=" not in mm.group(3):
                indent, ty, names = mm.groups()
                for nm in names.split(","):
                    decls.append("%s%s %s;" % (indent, ty, nm.strip()))
            else:
                decls.append(ln)
            i += 1
        else:
            break
    rest = "\n".join(lines[i:])
    return head, decls, rest


_FLIP = {"<": ">", ">": "<", "<=": ">=", ">=": "<="}


def mutate_cmps(text, rng):
    """Randomly toggle constant-comparison and relational-operand forms in body text."""
    def const_lt(m):
        v = int(m.group(2), 0)
        return "%s <= %d" % (m.group(1), v - 1) if rng.random() < 0.5 else m.group(0)
    def const_gt(m):
        v = int(m.group(2), 0)
        return "%s >= %d" % (m.group(1), v + 1) if rng.random() < 0.5 else m.group(0)
    text = re.sub(r"(\b[A-Za-z_]\w*)\s*<\s*(0x[0-9a-fA-F]+|\d+)\b", const_lt, text)
    text = re.sub(r"(\b[A-Za-z_]\w*)\s*>\s*(0x[0-9a-fA-F]+|\d+)\b", const_gt, text)

    # relational-OPERAND flip: `a < b` <-> `b > a` for variable/expression operands
    # (not constants — those are handled above). This changes the CMP operand order
    # MSVC emits (`cmp a,b;jl` vs `cmp b,a;jg`), the exact lever for functions whose
    # only residual is instruction-selection on a comparison (e.g. Zone::GetEdgeCode).
    def relflip(m):
        lhs, op, rhs = m.group(1), m.group(2), m.group(3)
        if re.fullmatch(r"0x[0-9a-fA-F]+|\d+", lhs) or re.fullmatch(r"0x[0-9a-fA-F]+|\d+", rhs):
            return m.group(0)                       # leave constant comparisons to the toggles above
        return "%s %s %s" % (rhs, _FLIP[op], lhs) if rng.random() < 0.5 else m.group(0)
    # operands: a bare identifier or `this->field` / `p.field` on each side
    operand = r"[A-Za-z_]\w*(?:\s*(?:->|\.)\s*[A-Za-z_]\w*)*"
    text = re.sub(r"(%s)\s*(<=|>=|<|>)\s*(%s)" % (operand, operand), relflip, text)
    return text


ORIG_LEN = None      # --len: the original's true extent (EH functions whose candidate length is off)


def compile_diff(full_text, addr, workdir, base, target_name, name_hint=None):
    cpp = os.path.join(workdir, base + ".cpp")
    obj = os.path.join(workdir, base + ".obj")
    open(cpp, "w").write(full_text)
    if os.path.exists(obj):
        os.remove(obj)
    r = subprocess.run([CL] + FLAGS + [base + ".cpp"], cwd=workdir, env=ENV, capture_output=True)
    if r.returncode != 0 or not os.path.exists(obj):
        return None
    foff = (addr - match.TEXT_VA) + match.TEXT_RAW
    best = None
    for name, code, relocs in match.coff_functions(obj):
        if target_name and name != target_name:
            continue
        # target discovery: pick the function whose mangled name matches the
        # source function name (?<name>@...), not the smallest-diff one.
        if target_name is None and name_hint and ("?%s@" % name_hint) not in name:
            continue
        L = match.trim_pad(code)
        tgt = ORIG_LEN if ORIG_LEN is not None else L
        orig = EXE[foff:foff + tgt]
        # Graded, register-rename-aware score (asmscore) as the oracle: gives the
        # hill-climb a real gradient instead of the flat raw-byte-diff plateau.
        res = asmscore.score(orig, code[:L], relocs, exact_len=tgt)
        cand = (0 if res.exact else 1, res.total, name, res.byte_diff)
        if best is None or cand < best:
            best = cand
    return best


# ---------------------------------------------------------------------------
# Statement-level reordering  (permuter transform: "statement-reordering")
# ---------------------------------------------------------------------------
# Splits a function body into top-level statement units, keeps declarations and
# control-flow blocks as anchors, and hill-climbs adjacent *independent* swaps.
# Declaration positions are preserved (so stack-slot offsets don't move — the
# inline-__asm functions reference locals by name, and their `_emit` bytes hard-
# code EBP offsets). Only executable statements are reordered, and only past a
# neighbour they don't data-depend on.
_ID = re.compile(r"[A-Za-z_]\w*")
_DECL_LHS = re.compile(
    r"^\s*(?:const\s+|volatile\s+|unsigned\s+|struct\s+)*"
    r"(?:int|short|char|long|float|double|bool|void|BITMAPINFOHEADER|BITMAPINFO|CDC|CFile|"
    r"Zone|ZoneObj|World|Canvas|GameView|[A-Z]\w*)[\s\*]+[A-Za-z_]\w*\s*$")


def _norm(s):
    return re.sub(r"\s+", "", s)


def _base(lval):
    m = _ID.search(lval)
    return m.group(0) if m else _norm(lval)


def _is_field(key):
    return "." in key or "->" in key or "[" in key


def split_statements(inner):
    """Split a function *body* (no enclosing braces) into top-level statement
    units. Control-flow constructs (if/for/while/do/switch/__asm) — whether they
    carry a brace block or a single trailing statement — come back as one unit."""
    units, i, n, bd, pd, start = [], 0, len(inner), 0, 0, 0
    typebrace = []                                                 # per-brace: is it a struct/union/enum body?
    _typekw = re.compile(r"\b(struct|union|enum|class)\b[^;{}]*$")
    while i < n:
        c = inner[i]
        if c == "/" and i + 1 < n and inner[i + 1] == "/":         # line comment
            j = inner.find("\n", i)
            i = n if j < 0 else j
            continue
        if c == "(":
            pd += 1
        elif c == ")":
            pd -= 1
        elif c == "{":
            typebrace.append(bool(_typekw.search(inner[start:i])))  # type body vs code block
            bd += 1
        elif c == "}":
            bd -= 1
            was_type = typebrace.pop() if typebrace else False
            if bd == 0 and not was_type:                          # code-block close -> statement boundary
                j = i + 1
                while j < n and inner[j] in " \t\n":
                    j += 1
                if inner[j:j + 4] == "else" or inner[j:j + 5] in ("while", "catch"):
                    pass                                            # attached else / do-while / catch
                else:
                    units.append(inner[start:i + 1])
                    start = i + 1
        elif c == ";" and bd == 0 and pd == 0:
            units.append(inner[start:i + 1])
            start = i + 1
        i += 1
    if inner[start:].strip():
        units.append(inner[start:])
    return [u for u in units if u.strip()]


def stmt_rw(stmt):
    """(writes, wbases, reads, kind) for a statement unit.
    kind in {decl, assign, ctrl1, ctrl, asm, other}; only assign/ctrl1 are movable.
    writes = full-lvalue keys (field granular); wbases/reads = base identifiers."""
    s = stmt.strip()
    if s.startswith("__asm"):
        ids = set(_ID.findall(s))
        return set(), ids, ids, "asm"
    if re.match(r"(if|for|while|do|switch)\b", s):
        m = re.match(r"if\s*\(([^)]*)\)\s*([^={;]+?)\s*=\s*([^=].*?);\s*$", s, re.S)
        if m and "{" not in s:                                     # simple `if (c) lval = rhs;`
            lhs = m.group(2)
            return ({_norm(lhs)}, {_base(lhs)},
                    set(_ID.findall(m.group(1))) | set(_ID.findall(m.group(3))), "ctrl1")
        ids = set(_ID.findall(s))
        return set(), ids, ids, "ctrl"
    m = re.match(r"(.+?)([-+*/&|^]?=)\s*([^=].*);\s*$", s, re.S)
    if m:
        lhs, op, rhs = m.group(1), m.group(2), m.group(3)
        if _DECL_LHS.match(lhs):                                   # declaration-with-init -> anchor
            nm = re.split(r"[\s\*]+", lhs.strip())[-1]
            return {_norm(nm)}, {_base(nm)}, set(_ID.findall(rhs)), "decl"
        w = {_norm(lhs)}
        wb = {_base(lhs)}
        r = set(_ID.findall(rhs))
        for x in re.findall(r"\[([^\]]*)\]", lhs):                 # index exprs on the LHS are reads
            r |= set(_ID.findall(x))
        if op != "=":                                             # compound assign reads LHS too
            r |= wb
        return w, wb, r, "assign"
    ids = set(_ID.findall(s))
    return set(), ids, ids, "other"


def _conflict(a, b):
    """True if statements a,b share a data hazard and must keep relative order."""
    wa, wba, ra, _ = a
    wb, wbb, rb, _ = b
    if wa & wb:                                                   # WAW on the same field/var
        return True
    if (wba & rb) or (wbb & ra):                                  # RAW / WAR on a base identifier
        return True
    common = wba & wbb                                            # same base written by both:
    if common and not (wa and wb and all(_is_field(k) for k in wa)
                       and all(_is_field(k) for k in wb)):
        return True                                              # ok only if both are distinct field writes
    return False


def hillclimb_stmts(text, addr, workdir, base, tgt):
    """Greedy adjacent-swap hill-climb over movable statements (declarations and
    control blocks stay put). Returns (best_text, best_diff)."""
    s, e = extract_func(text, addr)
    func = text[s:e]
    head, decls, rest = split_decls(func)
    inner = rest.rstrip()
    if inner.endswith("}"):
        inner = inner[:-1]                                        # drop the function's own close brace
    units = split_statements(inner)
    rw = [stmt_rw(u) for u in units]

    def build(order):
        body = head + "\n" + "\n".join(decls) + ("\n" if decls else "") \
            + "\n".join(units[k] for k in order) + "\n}"
        return text[:s] + body + text[e:]

    order = list(range(len(units)))
    r = compile_diff(build(order), addr, workdir, base, tgt)
    best = r[1] if r else 999
    seen = {tuple(order)}
    tried = 0
    improved = True
    while improved:
        improved = False
        for i in range(len(order) - 1):
            a, b = rw[order[i]], rw[order[i + 1]]
            if a[3] == "decl" and b[3] == "decl":                 # never reorder two decls (offsets)
                continue
            if a[3] not in ("assign", "ctrl1") and b[3] not in ("assign", "ctrl1"):
                continue
            if _conflict(a, b):
                continue
            order[i], order[i + 1] = order[i + 1], order[i]
            key = tuple(order)
            if key in seen:
                order[i], order[i + 1] = order[i + 1], order[i]
                continue
            seen.add(key)
            tried += 1
            r = compile_diff(build(order), addr, workdir, base, tgt)
            d = r[1] if r else 999
            if d < best:
                best = d
                improved = True
                print("  [stmt %d] swap %d<->%d  score=%d" % (tried, i, i + 1, best))
                if r and r[0] == 0:
                    return build(order), 0
            else:
                order[i], order[i + 1] = order[i + 1], order[i]   # revert
    return build(order), best


# ---------------------------------------------------------------------------
# Comparison-form hill-climb  (permuter transform: "comparison-flip")
# ---------------------------------------------------------------------------
# Each comparison in the body is an independent binary choice of *form* that MSVC
# 4.2 lowers literally: `a < b` -> `cmp a,b; jl` but `b > a` -> `cmp b,a; jg`, and
# `x < N` -> `cmp x,N; jl` but `x <= N-1` -> `cmp x,N-1; jle`. Same truth value,
# different bytes. Greedily flip one site at a time, keeping any flip the graded
# scorer likes. This is the lever for functions whose only residual is instruction
# selection on a comparison (e.g. Zone::GetEdgeCode). n independent binary knobs,
# so greedy single-flip converges in O(n) passes.
_CMP_SITE = re.compile(
    r"(?P<lhs>[A-Za-z_]\w*(?:\s*(?:->|\.)\s*[A-Za-z_]\w*)*|0x[0-9a-fA-F]+|\d+)"
    r"\s*(?P<op><=|>=|<|>)\s*"
    r"(?P<rhs>[A-Za-z_]\w*(?:\s*(?:->|\.)\s*[A-Za-z_]\w*)*|0x[0-9a-fA-F]+|\d+)")
_NUM = re.compile(r"^(?:0x[0-9a-fA-F]+|\d+)$")


def _cmp_variants(lhs, op, rhs):
    """All equivalent source forms of one comparison (original first)."""
    forms = ["%s %s %s" % (lhs, op, rhs)]
    if _NUM.match(rhs) and not _NUM.match(lhs):                    # constant on the right
        v = int(rhs, 0)
        if op == "<":
            forms.append("%s <= %d" % (lhs, v - 1))
        elif op == ">":
            forms.append("%s >= %d" % (lhs, v + 1))
        elif op == "<=":
            forms.append("%s < %d" % (lhs, v + 1))
        elif op == ">=":
            forms.append("%s > %d" % (lhs, v - 1))
    elif not _NUM.match(lhs) and not _NUM.match(rhs):              # var vs var -> operand flip
        forms.append("%s %s %s" % (rhs, _FLIP[op], lhs))
    return forms


def hillclimb_cmps(text, addr, workdir, base, tgt):
    """Greedy per-comparison form flip. Returns (best_text, best_score)."""
    s, e = extract_func(text, addr)
    func = text[s:e]
    ob = func.index("{")
    body = func[ob:]
    sites = [(m.start(), m.end(), m.group("lhs"), m.group("op"), m.group("rhs"))
             for m in _CMP_SITE.finditer(body)]
    # keep only sites with >1 form; remember each site's current chosen form
    sites = [(a, b, _cmp_variants(l, o, r)) for (a, b, l, o, r) in sites if len(_cmp_variants(l, o, r)) > 1]
    if not sites:
        return text, None
    choice = [0] * len(sites)

    def build():
        nb = []
        prev = 0
        for (a, b, forms), c in zip(sites, choice):
            nb.append(body[prev:a])
            nb.append(forms[c])
            prev = b
        nb.append(body[prev:])
        nf = func[:ob] + "".join(nb)
        return text[:s] + nf + text[e:]

    r = compile_diff(build(), addr, workdir, base, tgt)
    best = r[1] if r else 999
    tried = 0
    improved = True
    while improved:
        improved = False
        for i, (a, b, forms) in enumerate(sites):
            for c in range(len(forms)):
                if c == choice[i]:
                    continue
                old = choice[i]
                choice[i] = c
                tried += 1
                r = compile_diff(build(), addr, workdir, base, tgt)
                d = r[1] if r else 999
                if d < best:
                    best = d
                    improved = True
                    print("  [cmp %d] site %d -> '%s'  score=%d" % (tried, i, forms[c], best))
                    if r and r[0] == 0:
                        return build(), 0
                else:
                    choice[i] = old
    return build(), best


def main():
    src = sys.argv[1]
    addr = int(sys.argv[2], 0)
    iters = int(sys.argv[sys.argv.index("--iters") + 1]) if "--iters" in sys.argv else 3000
    mode = sys.argv[sys.argv.index("--mode") + 1] if "--mode" in sys.argv else "all"
    forced = sys.argv[sys.argv.index("--name") + 1] if "--name" in sys.argv else None
    if "--len" in sys.argv:
        global ORIG_LEN
        ORIG_LEN = int(sys.argv[sys.argv.index("--len") + 1], 0)
    text = open(src).read()
    afx = bool(re.search(r"#\s*include\s*<afx", text))
    if not afx:  # also look through local includes (e.g. Records.cpp -> Records.h -> <afxwin.h>)
        for inc in re.findall(r'#\s*include\s*"([^"]+)"', text):
            p = os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), inc)
            if os.path.exists(p) and re.search(r"#\s*include\s*<afx", open(p).read()):
                afx = True; break
    if afx:   # MFC TU -> needs _MBCS to compile afxwin.h
        FLAGS.extend(["/D", "_MBCS"])
    s, e = extract_func(text, addr)
    func = text[s:e]
    workdir = os.path.dirname(os.path.abspath(src))
    base = "_perm_%x" % addr

    # discover the target's mangled name with the original source. Identify by the
    # source function name (e.g. Canvas::Fill -> "Fill") so short functions can't
    # win the diff-minimisation; fall back to min-diff if unparsed.
    nm = re.search(r"::(\w+)\s*\(", func) or re.search(r"\b([A-Za-z_]\w*)\s*\(", func)
    src_name = nm.group(1) if nm else None
    b0 = compile_diff(text, addr, workdir, base, forced, src_name)
    if b0 is None:
        print("baseline does not compile")
        return 2
    target_name = b0[2]
    print("target=%s  baseline score=%d (bytes=%d)" % (target_name, b0[1], b0[3]))

    def cleanup():
        for f in (base + ".cpp", base + ".obj"):
            p = os.path.join(workdir, f)
            if os.path.exists(p):
                os.remove(p)

    def finish(t):
        out = src.replace(".cpp", ".matched.cpp")
        open(out, "w").write(t)
        print("*** MATCH -> %s" % out)
        cleanup()

    if b0[0] == 0:
        print("already byte-matches")
        cleanup()
        return 0

    best_text, best = text, b0[1]

    # Phase 1 — statement reordering (dependency-safe; declarations untouched)
    if mode in ("all", "stmt"):
        best_text, best = hillclimb_stmts(best_text, addr, workdir, base, target_name)
        print("after statement hill-climb: score=%d" % best)
        if best == 0:
            finish(best_text)
            return 0

    # Phase 1b — comparison-form hill-climb (operand flip / const-form toggle)
    if mode in ("all", "cmp"):
        t2, c2 = hillclimb_cmps(best_text, addr, workdir, base, target_name)
        if c2 is not None:
            best_text, best = t2, c2
            print("after comparison hill-climb: score=%d" % best)
            if best == 0:
                finish(best_text)
                return 0

    # Phase 2 — declaration order x comparison form (original search)
    if mode in ("all", "decl"):
        s2, e2 = extract_func(best_text, addr)
        func2 = best_text[s2:e2]
        head, decls, rest = split_decls(func2)
        rng = random.Random(1234)
        perms = list(itertools.permutations(range(len(decls)))) if len(decls) <= 6 else None
        tried = 0

        def build(order, cmp_seed):
            d = [decls[i] for i in order]
            body = mutate_cmps(rest, random.Random(cmp_seed))
            nf = head + "\n" + "\n".join(d) + ("\n" if d else "") + body
            return best_text[:s2] + nf + best_text[e2:]

        seen = set()
        orders = perms if perms is not None else \
            [tuple(rng.sample(range(len(decls)), len(decls))) for _ in range(iters)]
        for order in orders:
            for cmp_seed in range(6):
                cand = build(order, cmp_seed)
                if cand in seen:
                    continue
                seen.add(cand)
                tried += 1
                r = compile_diff(cand, addr, workdir, base, target_name)
                if r is None:
                    continue
                if r[1] < best:
                    best, best_text = r[1], cand
                    print("  [decl %d] new best score=%d (bytes=%d)  (order=%s)" % (tried, best, r[3], order))
                if r[0] == 0:
                    finish(cand)
                    return 0
                if tried >= iters:
                    break
            if tried >= iters:
                break

    print("no exact match; best score=%d" % best)
    cleanup()
    return 1


if __name__ == "__main__":
    sys.exit(main())
