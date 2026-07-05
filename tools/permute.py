#!/usr/bin/env python3
"""Permuter for the Yodecomp matching decomp (VC++ 4.2 under wine).

Given a .cpp and a target function address, it searches semantically-equivalent
source variations and compiles each with cl 4.2, using the relocation-masked
byte-match against the original as the oracle. Stops when a variant byte-matches.

Transformations (the ones proven to move MSVC 4.2 codegen):
  * permute the leading local-declaration block   (-> register / x87-slot allocation)
  * toggle constant comparison form  `x < N` <-> `x <= N-1`, `x > N` <-> `x >= N+1`
  * flip relational operands          `a < b` <-> `b > a`   (cheap; sometimes helps)

Usage:  tools/permute.py src/Zone/Zone.cpp 0x405330 [--iters 4000]

The target function is the one whose `// FUNCTION: YODA 0xADDR` marker matches.
Only that function's body is mutated; the rest of the file is kept (TU context).
"""
import sys, os, re, random, subprocess, itertools
import functools
print = functools.partial(print, flush=True)

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match

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
    return text


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
        orig = EXE[foff:foff + L]
        cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
        diffs = sum(1 for i in range(min(len(cm), len(om))) if cm[i] != om[i])
        cand = (0 if (diffs == 0 and len(orig) == L) else 1, diffs, name)
        if best is None or cand < best:
            best = cand
    return best


def main():
    src = sys.argv[1]
    addr = int(sys.argv[2], 0)
    iters = int(sys.argv[sys.argv.index("--iters") + 1]) if "--iters" in sys.argv else 3000
    text = open(src).read()
    s, e = extract_func(text, addr)
    func = text[s:e]
    head, decls, rest = split_decls(func)
    workdir = os.path.dirname(os.path.abspath(src))
    base = "_perm_%x" % addr

    # discover the target's mangled name with the original source.
    # Identify by the source function name (e.g. Canvas::Fill -> "Fill") so short
    # functions can't win the diff-minimisation; fall back to min-diff if unparsed.
    nm = re.search(r"::(\w+)\s*\(", func) or re.search(r"\b([A-Za-z_]\w*)\s*\(", func)
    src_name = nm.group(1) if nm else None
    b0 = compile_diff(text, addr, workdir, base, None, src_name)
    if b0 is None:
        print("baseline does not compile"); return 2
    target_name = b0[2]
    print("target=%s  baseline diff=%d  decls=%d" % (target_name, b0[1], len(decls)))

    rng = random.Random(1234)
    perms = list(itertools.permutations(range(len(decls)))) if len(decls) <= 6 else None
    best = b0[1]
    best_text = text
    tried = 0
    def build(order, cmp_seed):
        d = [decls[i] for i in order]
        body = mutate_cmps(rest, random.Random(cmp_seed))
        nf = head + "\n" + "\n".join(d) + ("\n" if d else "") + body
        return text[:s] + nf + text[e:]

    seen = set()
    orders = perms if perms is not None else [tuple(rng.sample(range(len(decls)), len(decls))) for _ in range(iters)]
    for order in orders:
        for cmp_seed in range(6):            # a few comparison-form combos per ordering
            cand_text = build(order, cmp_seed)
            if cand_text in seen:            # skip duplicate variants (no-op mutations)
                continue
            seen.add(cand_text)
            tried += 1
            r = compile_diff(cand_text, addr, workdir, base, target_name)
            if r is None:
                continue
            if r[1] < best:
                best, best_text = r[1], cand_text
                print("  [%d] new best diff=%d  (order=%s)" % (tried, best, order))
            if r[0] == 0:
                out = src.replace(".cpp", ".matched.cpp")
                open(out, "w").write(cand_text)
                print("*** MATCH after %d tries -> %s" % (tried, out))
                for f in (base + ".cpp", base + ".obj"):
                    p = os.path.join(workdir, f)
                    if os.path.exists(p): os.remove(p)
                return 0
            if tried >= iters:
                break
        if tried >= iters:
            break
    print("no exact match in %d tries; best diff=%d" % (tried, best))
    for f in (base + ".cpp", base + ".obj"):
        p = os.path.join(workdir, f)
        if os.path.exists(p): os.remove(p)
    return 1


if __name__ == "__main__":
    sys.exit(main())
