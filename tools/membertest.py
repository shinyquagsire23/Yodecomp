#!/usr/bin/env python3
"""Member-vs-file-scope A/B (docs/compiler-hunt.md v96 ndash; pickup step 2).

v96 proved *file-scope* declaration count moves the dial (MSVC 4.2 regalloc) even when
unreferenced. PLAN_COMPLETED lesson #8 (v36) claims an unreferenced *member* decl is INERT.
That reconciliation was never tested. This tool settles it on a single TU (Worldgen.cpp, ~7
symbols short per headersweep.py).

Mechanism under test: if the dial counts ONLY file-scope symbols (the v96 hypothesis), then
adding N members to an UNUSED struct (one constant file-scope TAG symbol) is inert, while N
file-scope `extern` decls move it. If members DO move it, the missing 7 could be class fields
and the search broadens.

The struct host is never referenced, so its size never reaches any function body -- the only
dial input is the symbol count. Header is restored via atexit+finally (same discipline as
dialsweep.py). Do NOT run concurrently with any other sweep / progress.py.

Usage:
  tools/membertest.py [--tu src/Worldgen.cpp] [--header src/Worldgen.h] [--max 12]

⚠ v101: this tool measures through dialsweep.exact_set(), which until v101 carried the
pre-v100 COMDAT filter and therefore the 28-mis-pair positional cascade. EVERY RESULT THIS
TOOL PRODUCED BEFORE v101 IS UNVERIFIED and should be re-run before being cited (notably
v97's "members are INERT", which v99 already contradicted). See docs/compiler-hunt.md v101.
"""
import os, sys, atexit, shutil

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dialsweep as ds          # exact_set() + install()

ROOT = ds.ROOT
MARK = "// ==== MEMBERTEST GENERATED BLOCK (tools/membertest.py) workload ===="


def extern_block(n):
    if n == 0:
        return ""
    return "\n".join([MARK] + ["extern int g_mtExtern%d;" % i for i in range(n)] + [MARK]) + "\n"


def member_block(n):
    # One unused struct whose MEMBER count = n. The file-scope TAG is constant; only the
    # number of member declarations varies. Host is never referenced -> size is irrelevant.
    if n == 0:
        return ""
    mems = " ".join("int m%d;" % i for i in range(n))
    return "\n".join([MARK, "struct DialMemHost { %s };" % mems, MARK]) + "\n"


def main():
    def opt(flag, dflt):
        return sys.argv[sys.argv.index(flag) + 1] if flag in sys.argv else dflt
    tu = os.path.join(ROOT, opt("--tu", "src/Worldgen.cpp"))
    header = os.path.join(ROOT, opt("--header", "src/Worldgen.h"))
    nmax = int(opt("--max", "12"))

    orig_text = open(header).read()
    backup = header + ".membertest.bak"
    shutil.copyfile(header, backup)

    def restore():
        try:
            open(header, "w").write(orig_text)
            if os.path.exists(backup):
                os.remove(backup)
        except Exception as e:
            print("!! RESTORE FAILED: %s (backup at %s)" % (e, backup), file=sys.stderr)
    atexit.register(restore)

    def run(label, block):
        ds.install(header, orig_text, block)
        cur = ds.exact_set(tu)
        return None if cur is None else {va for va, ok in cur.items() if ok}

    try:
        base = run("baseline", "")
        if base is None:
            raise SystemExit("baseline compile failed")
        nb = len(base)
        print("tu=%s header=%s" % (os.path.relpath(tu, ROOT), os.path.relpath(header, ROOT)))
        print("baseline: %d/%d exact\n" % (nb, nb))
        print("n   extern(moves-if-file-scope)   members(inert-if-file-scope)   extern-only-gain")
        print("--  ---------------------------   ----------------------------   --------------")
        for n in range(0, nmax + 1):
            ex = run("ext%d" % n, extern_block(n))
            mb = run("mem%d" % n, member_block(n))
            if ex is None or mb is None:
                print("%-3d COMPILE FAILED" % n); continue
            sext = ex - base
            print("%-3d %-28s %-30s %s" % (
                n,
                "%d/%d%s" % (len(ex), nb, _suffix(sext, nb)),
                "%d/%d%s" % (len(mb), nb, _suffix(mb - base, nb)),
                ",".join("%#x" % a for a in sorted(sext)) or "-",
            ))
    finally:
        restore()
        atexit.unregister(restore)

    print("\nInterpretation: members moving with externs => class fields are legal dial "
          "inputs; members flat while externs move => the missing N are FILE-SCOPE only.")


def _suffix(diff, nb):
    return "  (+%d)" % len(diff) if diff else "        "


if __name__ == "__main__":
    main()
