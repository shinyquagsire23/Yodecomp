#!/usr/bin/env python3
"""Sweep the ORDER of a function's leading function-scope declaration block. A/B each permutation.

⭐ THE LEVER (lesson #38, and the v108 gap this tool closes). `hoisttest.py` asks "should this
inner-block local be HOISTED?" — a yes/no question per declaration. It never permutes the decls
that are ALREADY at function scope, yet that order is itself a register-allocation dial:

  ParseTilesMaybe 0x41a030 (v108) sat at DIFF(3) for the whole project. Every loop-form spelling
  measured 3 B dead flat (lesson #40 refuted) and the compare's operand order was inert — but
  simply declaring `int i;` BEFORE `int n = nBytes / 0x404;` made it byte-exact. Every order with
  `n` before `i` gives 40-43 B. Nothing else changed.

  Precedent: PlaceZoneObjectTiles (v104) is exact with `Zone *z` before `ZoneObj *o` and 22 B
  after; SetCurrentToIntroZone (v107) needs `i` before `pZone`.

⚠ A FLAT sweep from hoisttest.py or a loop-form probe is NOT a dead end — run this before parking.
The three dials compose: decl SCOPE (#37), decl SET+ORDER (#38), loop FORM (#40).

Measurement is delegated to tools/vartest.py verbatim — this only GENERATES the spellings, so the
anchor's exact predicate and the --expect baseline guard are inherited, not re-derived (the
v100/v101 rule: a harness must agree with progress.py at zero perturbation).

Permutations are LINE-NEUTRAL by construction: the block keeps its original line count (blank
lines are re-appended), so lesson #23 can't confound the measurement.

Usage:
    tools/declorder.py <tu.cpp> <0xADDR> --expect N [--inner] [--max-perm K] [--keep]

--inner (v118) permutes EVERY decl run, not just the leading function-scope one. That axis
landed AddHealth 0x427690 and no other tool in the project can reach it. ⚠ v132: the v118 scan
only saw a run that OPENS a brace-block, so a run sitting after a statement (`if (...) {...}`
then `int nObjs = ...; int j = 0;`) was invisible — see inner_blocks' note.

--expect N is REQUIRED (get it from tools/bytediff.py). K caps the permutation count (default
120); with more decls than that fits, a random-free deterministic subset (adjacent swaps plus
full reversal) is used instead.
"""
import os, re, sys, itertools, subprocess, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TYPES = (r"int|short|char|long|float|double|void|BOOL|UINT|DWORD|WORD|BYTE|LONG|"
         r"[A-Z]\w+")
# a whole single-line declaration, capturing the declared name for labelling.
# The trailing declarator is either an ARRAY extent (`char buf[32];` — v109: these were
# silently unmatched, and because a buffer is so often the FIRST local the block detector
# bailed on line 1 and reported "no permutable block" for 9 residuals, three of them the
# SaveStoryHistory* family) or an optional initializer.
# ONE declaration, NOT anchored to end-of-line (v111: the old regex demanded `;\s*$`, so a line
# carrying SEVERAL declarations -- `ZoneObj *o; int count;`, the line-neutral idiom this project
# uses everywhere to keep sweeps from rotating the dial (lesson #23) -- matched NOTHING, the block
# detector bailed on line 1, and the tool reported "nothing to permute". Same shape as the v109
# array-extent bug and the v110 epilogue-pops bug: a harness reporting NOTHING TO DO is as suspect
# as one reporting a finding.
ONE_DECL = re.compile(r"\s*(?:const\s+)?(?:unsigned\s+|signed\s+)?(?:%s)[\s\*]+(\w+)"
                      r"(?:\s*\[[^\];]*\])*"
                      r"(?:\s*=[^;]*)?;" % TYPES)
DECL = re.compile(r"^\s+(?:const\s+)?(?:unsigned\s+|signed\s+)?(?:%s)[\s\*]+(\w+)"
                  r"(?:\s*\[[^\];]*\])*"
                  r"(?:\s*=[^;]*)?;\s*$" % TYPES)
# things that end the leading decl block even though they look declaration-ish
BAD = re.compile(r"\breturn\b|\bcase\b|\bgoto\b|::|\(\s*\)|\w+\s*\(")


def decl_names(s):
    """Names declared by line `s` if the WHOLE line is declarations, else None.

    Handles the several-declarations-on-one-line idiom; the line stays ONE permutable unit
    (whole lines are swapped), so within-line order is NOT explored -- hand-sweep that with
    tools/vartest.py, per lesson #38.
    """
    names, pos = [], 0
    while pos < len(s):
        if not s[pos:].strip():
            break
        m = ONE_DECL.match(s, pos)
        if not m:
            return None
        seg = s[pos:m.end()]
        if BAD.search(seg) and "=" not in seg:   # a call-shaped segment is not a decl
            return None
        if "," in seg.split("=")[0]:             # multi-declarator: not safely permutable
            return None
        names.append(m.group(1))
        pos = m.end()
    return names or None


def leading_block(body):
    """[(line_index, label)] for the run of declarations at the top of the body."""
    out = []
    for k in range(1, len(body)):
        s = body[k].split("//")[0].rstrip()
        if not s.strip():
            if out: break                     # blank line ends the block
            continue
        names = decl_names(s)
        if names is None:
            break
        out.append((k, "+".join(names)))
    return out


def inner_blocks(body):
    """[(label, [(line_index, name), ...]), ...] for EVERY maximal run of >= 2 decl lines.

    ⭐ v118 (lesson #50's enabler): this tool only ever permuted the LEADING function-scope
    block, so an inner block's decl order — inside an `if`/loop body — was unreachable by any
    harness in the project (`hoisttest.py` asks about SCOPE, not order). That axis is real and
    it is not small: `AddHealth` 0x427690 became byte-EXACT only with the death tail's inner
    block ordered pTile,bFound,i; the other five orders give 117-421 B.

    ⚠ v132 CLOSED A REAL UNDER-REPORT — the same family as the v109 array-extent and v111
    several-decls-on-one-line bugs, and the third time this one tool has told a session there
    was less to permute than there is. The v118 scan only STARTED a run at a line that is
    exactly `{`, i.e. it saw a block's LEADING decl run and nothing else. A run that opens
    mid-block — the shape `if (...) { ... }` followed by `int nObjs = ...; int j = 0;`, which
    `PlaceZone` 0x4260e0 carries THREE times — was invisible, and v131's "24 permutations,
    flat" verdict on that function was computed over a strictly smaller seam than exists.
    Any maximal run of consecutive declaration-only lines is permutable (a non-decl line, a
    blank line and a brace all break the run), so scan for the runs directly and drop the
    brace precondition. body[0] is the function's own `{`, so the leading block still comes
    out of this scan.
    """
    out, k, n = [], 0, len(body)
    while k < n:
        s = body[k].split("//")[0].rstrip()
        if not s.strip() or decl_names(s) is None:
            k += 1
            continue
        run, m = [], k
        while m < n:
            t = body[m].split("//")[0].rstrip()
            if not t.strip():
                break                         # blank line ends the run
            names = decl_names(t)
            if names is None:
                break
            run.append((m, "+".join(names)))
            m += 1
        if len(run) >= 2:
            out.append(("block@%d" % run[0][0], run))
        k = max(m, k + 1)
    return out


def deps_of(blk, body):
    """{slot: set(slots it must follow)} — a decl whose INITIALIZER mentions a name declared
    earlier in the same block cannot be moved ahead of it.

    ⭐ v118: without this the sweep spends most of its compiles on permutations that cannot
    build (`int idw = ...; int id = idw + 1;` reversed), and reports a wall of COMPILE FAILED.
    On 0x41a1c0 that was 16 of 23 permutations — honest, but pure waste, and it buried the
    real (flat) result.
    """
    names = {}
    for slot, (k, label) in enumerate(blk):
        for nm in label.split("+"):
            names[nm] = slot
    deps = {}
    for slot, (k, label) in enumerate(blk):
        init = body[k].split("=", 1)[1] if "=" in body[k] else ""
        # ⚠ strip MEMBER names first: `mapGrid[i].id` would otherwise make the local `id`
        # look like a dependency of `idw`, and the resulting phantom CYCLE skipped all 23
        # permutations of 0x41a1c0 — a harness reporting "nothing to do" (v109/v111 family).
        init = re.sub(r"(?:\.|->)\s*\w+", "", init)
        need = set()
        for nm in re.findall(r"\b\w+\b", init):
            if nm in names and names[nm] != slot:
                need.add(names[nm])
        deps[slot] = need
    return deps


def base_order_of(blk):
    return tuple(range(len(blk)))


def legal(p, deps):
    """True if permutation p (p[k] = which decl lands in slot k) respects every dependency."""
    pos = {src: k for k, src in enumerate(p)}
    return all(pos[d] < pos[slot] for slot, ds in deps.items() for d in ds)


def permutations_of(n, maxp):
    perms = list(itertools.permutations(range(n)))
    if len(perms) > maxp:                      # deterministic fallback: swaps + reversal
        perms = [tuple(range(n))]
        for a in range(n - 1):
            p = list(range(n)); p[a], p[a + 1] = p[a + 1], p[a]; perms.append(tuple(p))
        for a in range(n):                     # each decl rotated to the front and to the back
            p = [x for x in range(n) if x != a]
            perms.append(tuple([a] + p)); perms.append(tuple(p + [a]))
        perms.append(tuple(reversed(range(n))))
    return perms


def main():
    args = sys.argv[1:]
    expect = None
    maxp = 120
    keep = "--keep" in args
    inner = "--inner" in args
    args = [a for a in args if a not in ("--keep", "--inner")]
    for flag in ("--expect", "--max-perm"):
        if flag in args:
            i = args.index(flag)
            v = int(args[i + 1], 0); del args[i:i + 2]
            if flag == "--expect": expect = v
            else: maxp = v
    if len(args) != 2 or expect is None:
        raise SystemExit(__doc__)
    cpp, addr = args[0], args[1]
    path = cpp if os.path.isabs(cpp) else os.path.join(ROOT, cpp)

    lines = open(path, encoding="utf-8").read().split("\n")
    mk = "0x%08x" % int(addr, 16)
    i = next(k for k, l in enumerate(lines) if re.search(r"FUNCTION:\s*YODA\s+" + mk, l))
    o = next(k for k in range(i, len(lines)) if lines[k] == "{")
    j = next(k for k in range(o, len(lines)) if lines[k] == "}")
    body = lines[o:j + 1]

    if inner:
        blocks = inner_blocks(body)
        if not blocks:
            raise SystemExit("%s has no run of >= 2 declarations — nothing to permute"
                             % mk)
    else:
        blk = leading_block(body)
        if len(blk) < 2:
            raise SystemExit("%s has %d leading function-scope decl(s) — nothing to permute"
                             % (mk, len(blk)))
        blocks = [("leading", blk)]

    for label, blk in blocks:
        print("%s decl block (%d):" % (label, len(blk)))
        for k, n in blk:
            print("   %-14s  %s" % (n, body[k].strip()[:66]))

    seen, variants = set(), []
    budget = max(1, maxp // len(blocks))
    for label, blk in blocks:
        idx = [k for k, _ in blk]
        n = len(blk)
        base_order = tuple(range(n))
        deps = deps_of(blk, body)
        # self-check: the SOURCE's own order must satisfy the dependencies it just derived.
        # If it does not, the dependency scan is wrong -- disable it rather than silently
        # skipping every permutation (v100/v101: a tool must agree with reality at zero
        # perturbation before any of its filtering means anything).
        if not legal(base_order_of(blk), deps):
            print("   (%s: dependency self-check FAILED — filter disabled)" % label)
            deps = {}
        skipped = 0
        for p in permutations_of(n, budget):
            if p == base_order:
                continue                       # vartest measures the unmodified body as BASELINE
            if not legal(p, deps):
                skipped += 1                   # would not compile: an initializer dependency
                continue
            b = list(body)
            for slot, src in zip(idx, p):
                b[slot] = body[idx[src]]
            text = "\n".join(b)
            if text in seen:
                continue
            seen.add(text)
            name = ",".join(blk[s][1] for s in p)[:30]
            variants.append(("%s:%s" % (label, name) if inner else name, text))
        if skipped:
            print("   (%s: %d permutation(s) skipped — initializer dependency)"
                  % (label, skipped))

    print("\n%d permutations\n" % len(variants))

    # NOT under tools/ — an interrupted run would leave a stray tmp*.py in the repo
    vf = tempfile.NamedTemporaryFile("w", suffix=".py", prefix="yoda_declord_", delete=False)
    vf.write("BASE = %r\nVARIANTS = %r\n" % ("\n".join(body), variants))
    vf.close()
    try:
        rc = subprocess.call([sys.executable, os.path.join(ROOT, "tools", "vartest.py"),
                              cpp, addr, vf.name, "--expect", str(expect)])
    finally:
        if keep: print("[variants kept: %s]" % vf.name)
        else: os.unlink(vf.name)
    return rc


if __name__ == "__main__":
    sys.exit(main())
