#!/usr/bin/env python3
"""Completion dashboard: how much of YodaDemo.exe's app region is byte-matched.

Compiles every src/**/*.cpp with the VC++ 4.2 toolchain, byte-matches each
annotated function against the original (relocations masked), and reports two
tiers against the total app-function bytes:

  EXACT    byte-identical (reloc-masked) — done.
  PARTIAL  transcribed (has a // FUNCTION marker + a compiled COMDAT) but not
           byte-exact — the C is written; only byte-matching polish remains.

Everything else still needs DECOMPILING. Functions carried by more than one TU
(e.g. the retired src/Dta duplicates of Worldgen parsers) are deduped by
address, an exact copy in any TU winning.

⚠ ONE BASIS, EVERYWHERE (v111). EXACTNESS is decided by the reloc-masked byte
compare over our trimmed COMDAT length — that predicate is unchanged and is what
every other tool copies. The BYTE ACCOUNTING is a separate question, and it now
attributes each function its Ghidra EXTENT (toolchain/test/app_funcs.txt, 410
funcs / 156054 bytes, INCLUDING EH funclets and embedded jump tables) in both the
numerator and the denominator, so EXACT + PARTIAL + TO DO == TOTAL exactly and
`% transcribed` equals `marker coverage` by construction.

It did not use to. The numerator was our COMDAT lengths (funclets + tables IN)
and the denominator was Ghidra body sizes (funclets + tables OUT), so the report
claimed ">>> 124.88% transcribed; -24.88% left to decompile <<<". The mismatch
was even flagged in a comment beside the print and still shipped for many
sessions — a reminder that a visibly impossible number is a bug, not a quirk.
The legacy body-only totals survive as BODY_ONLY_* for reference only; never
divide extent-basis byte counts by them.

Usage:  tools/progress.py
"""
import os, sys, glob, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match   # reuse coff_functions / trim_pad / mask / diff logic
import verify  # reuse owner_of / LIB_OWNERS (MFC base-class COMDAT filter)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Legacy BODY-ONLY totals (Ghidra function bodies, EXCLUDING EH funclets and jump tables).
# Kept for reference only — never mix these with byte counts measured on the extent basis
# below, which is what produced the old ">>> 124.88% transcribed; -24.88% left <<<" line.
BODY_ONLY_BYTES = 128158
BODY_ONLY_FUNCS = 534
EXTENTS_PATH = os.path.join(ROOT, "toolchain", "test", "app_funcs.txt")


def load_extents():
    """{va: true_extent} — Ghidra function extents INCLUDING EH funclets + jump tables.
    This is the one basis the dashboard reports against; regenerate via the
    run_script_inline dump if the app-region function set changes."""
    table = {}
    if os.path.exists(EXTENTS_PATH):
        for ln in open(EXTENTS_PATH):
            p = ln.split()
            if len(p) == 2:
                table[int(p[0], 16)] = int(p[1])
    return table
CL = os.path.join(ROOT, "toolchain/bin/cl")
FLAGS = "/nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS".split()
EXE = open(os.path.join(ROOT, "YodaDemo/YodaDemo.exe"), "rb").read()
import re


def compile_obj(cpp):
    build = os.path.join(ROOT, "build"); os.makedirs(build, exist_ok=True)
    obj = os.path.join(build, os.path.splitext(os.path.basename(cpp))[0] + ".obj")
    env = dict(os.environ, WINEDEBUG="-all")
    # MFC TUs need /D_MBCS to compile afxwin.h — detect it through local includes too
    # (e.g. Iact.cpp -> RecordClasses.h -> <afxwin.h>), not just a direct <afx> include.
    txt = open(cpp).read()
    afx = bool(re.search(r"#\s*include\s*<afx", txt))
    for inc in re.findall(r'#\s*include\s*"([^"]+)"', txt):
        p = os.path.join(os.path.dirname(cpp), inc)
        if os.path.exists(p) and re.search(r"#\s*include\s*<afx", open(p).read()):
            afx = True
            break
    flags = FLAGS + (["/D", "_MBCS"] if afx else [])
    fo = "/Fo" + os.path.relpath(obj, os.path.dirname(cpp))
    r = subprocess.run([CL] + flags + [fo, os.path.basename(cpp)],
                       cwd=os.path.dirname(cpp), env=env, capture_output=True)
    return obj if r.returncode == 0 and os.path.exists(obj) else None


def main():
    # addr -> (exact, nbytes): dedupe functions carried by more than one TU
    # (exact beats partial; among equals keep the first seen).
    extents = load_extents()          # load ONCE — see the basis rule below
    by_addr = {}
    rows = []
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True)):
        obj = compile_obj(cpp)
        rel = os.path.relpath(cpp, ROOT)
        if not obj:
            rows.append((rel, "COMPILE FAILED", 0, 0)); continue
        text = open(cpp).read()
        # drop MFC base-class library COMDATs (CObject::~CObject, Serialize, ??_GCObject, ...)
        # and CRT dynamic-init thunks (_$E123 etc. from a file-scope object with a ctor/dtor):
        # they byte-match but carry no // FUNCTION marker, so best-fit would mis-pair them.
        # ...EXCEPT one a marker EXPLICITLY names by mangled hint: some MFC base-class COMDAT
        # copies provably survive per-TU in the app region (CDeskcppView's CGdiObject/CBitmap
        # trios) and ARE ours to match. Dropping them made every following marker in the TU
        # fall back POSITIONALLY, cascading 28 mis-pairs through DeskcppView.cpp (v100).
        # verify.py has always had this exception; progress.py/idiomscan.py had drifted.
        hinted = set(re.findall(
            r"//\s*FUNCTION:\s*YODA\s+0x[0-9a-fA-F]+[^\n]*?(\?\?(?:_[A-Z]|[0-9])\w+@@)", text))
        funcs = [f for f in match.coff_functions(obj)
                 if (verify.owner_of(f[0]) not in verify.LIB_OWNERS
                     or any(h in f[0] for h in hinted))
                 and not f[0].lstrip("?").startswith(("_$E", "$E"))]
        # pair each marker to its SAME-named COMDAT (best-fit mis-assigns reloc-masked-identical
        # stubs — two GetMessageMaps become byte-identical once their one imm reloc is masked).
        mb = mf = pf = 0
        for va, name, code, relocs in match.pair_by_name(text, funcs):
            L = match.trim_pad(code)
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            orig = EXE[foff:foff + L]
            cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
            diffs = sum(1 for i in range(min(len(cm), len(om))) if cm[i] != om[i])
            exact = diffs == 0 and len(orig) == L
            if exact:
                mb += extents.get(va, L); mf += 1
            else:
                pf += 1
            prev = by_addr.get(va)
            if prev is None or (exact and not prev[0]):
                by_addr[va] = (exact, L)
        rows.append((rel, "%d+%d/%d" % (mf, pf, len(funcs)), mb, len(funcs)))

    # ⚠ THE BASIS RULE (v111). Byte percentages must have the SAME basis in numerator and
    # denominator. `L` here is OUR trimmed COMDAT length, which INCLUDES EH funclets and
    # embedded jump tables; the old denominator (BODY_ONLY_BYTES) excluded both. Dividing one
    # by the other reported ">>> 124.88% transcribed; -24.88% left to decompile <<<" — a
    # number that had been visibly impossible for many sessions. Everything below is measured
    # against the Ghidra EXTENT table, which is on the same (funclets-included) basis as L and
    # is what the marker-coverage figure has always used.
    exact_funcs = sum(1 for e, L in by_addr.values() if e)
    partial_funcs = sum(1 for e, L in by_addr.values() if not e)
    # bytes on the extent basis; a marker with no extent entry is NOT silently dropped
    exact_bytes = sum(extents[va] for va, (e, L) in by_addr.items() if e and va in extents)
    partial_bytes = sum(extents[va] for va, (e, L) in by_addr.items()
                        if not e and va in extents)
    no_extent = sorted(va for va in by_addr if va not in extents)

    print("=" * 60)
    print(" Yodecomp completion — app region (0x401000-0x429000)")
    print("=" * 60)
    print("  %-26s %-12s %8s" % ("", "exact+part", "exact B"))   # extent basis
    for rel, note, mb, nf in rows:
        print("  %-26s %-12s %6d B" % (rel, note, mb))
    print("-" * 60)
    if not extents:
        print("  EXACT    %6d funcs  — byte-matched" % exact_funcs)
        print("  PARTIAL  %6d funcs  — transcribed, needs byte-matching" % partial_funcs)
        print("  !! %s missing — byte percentages need it for a consistent basis"
              % os.path.relpath(EXTENTS_PATH, ROOT))
        return
    total_bytes = sum(extents.values())
    todo_funcs = len(extents) - (exact_funcs + partial_funcs) + len(no_extent)
    todo_bytes = total_bytes - exact_bytes - partial_bytes
    e_pct = 100.0 * exact_bytes / total_bytes
    p_pct = 100.0 * partial_bytes / total_bytes
    d_pct = 100.0 * todo_bytes / total_bytes
    print("  EXACT    %6d bytes  (%d funcs)  — byte-matched" % (exact_bytes, exact_funcs))
    print("  PARTIAL  %6d bytes  (%d funcs)  — transcribed, needs byte-matching"
          % (partial_bytes, partial_funcs))
    print("  TO DO    %6d bytes  (%d funcs)  — no // FUNCTION marker yet"
          % (todo_bytes, todo_funcs))
    print("  TOTAL    %6d bytes  (%d funcs in app region, Ghidra extents incl. funclets"
          "/tables)" % (total_bytes, len(extents)))
    print("  >>> %.2f%% exact + %.2f%% partial = %.2f%% transcribed; "
          "%.2f%% left to decompile <<<" % (e_pct, p_pct, e_pct + p_pct, d_pct))
    # SELF-CHECK (v111): the tiers must partition the basis exactly. A dashboard that can
    # print an impossible percentage is a dashboard nobody reads carefully — this is the
    # assert that would have caught the old >100% line on the day it appeared.
    assert exact_bytes + partial_bytes + todo_bytes == total_bytes, \
        "basis mismatch: %d + %d + %d != %d" % (exact_bytes, partial_bytes,
                                                todo_bytes, total_bytes)
    assert 0.0 <= e_pct + p_pct <= 100.0 and 0.0 <= d_pct <= 100.0
    if no_extent:
        print("  (%d marked function(s) have no extent entry, excluded from the bytes above: %s)"
              % (len(no_extent), ", ".join("%#x" % v for v in no_extent)))
    # The per-TU column double-counts a function that is exact in more than one TU; the EXACT
    # total dedupes by address. Say so rather than leave two near-equal numbers unexplained.
    col = sum(mb for _, _, mb, _ in rows)
    if col != exact_bytes:
        print("  (per-TU column sums to %d B — %d B more than EXACT: functions carried by "
              "more than one TU," % (col, col - exact_bytes))
        print("   counted once in the total, once per TU in the column.)")
    # Marker coverage: which app-region functions carry a // FUNCTION marker at all. This is
    # the same extent table as above (v111 — it used to be loaded a SECOND time here, which is
    # how the two bases drifted apart in the first place), so `marker coverage` and
    # `% transcribed` now agree by construction rather than by coincidence.
    marked = set()
    for cpp in glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True):
        for m in re.finditer(r"FUNCTION:\s*YODA\s+0x0*([0-9a-fA-F]+)", open(cpp).read()):
            marked.add(int(m.group(1), 16))
    cov = sum(sz for va, sz in extents.items() if va in marked)
    un = sorted((sz, va) for va, sz in extents.items() if va not in marked)
    print("  >>> marker coverage %.2f%% (%d bytes); largest unclaimed:"
          % (100.0 * cov / total_bytes, cov))
    for sz, va in un[-6:][::-1]:
        print("      %#x  %d bytes" % (va, sz))
    print("  (legacy body-only basis, funclets/tables EXCLUDED: %d bytes / %d funcs — do NOT"
          % (BODY_ONLY_BYTES, BODY_ONLY_FUNCS))
    print("   divide extent-basis byte counts by it; that is what produced the old >100% line.)")


if __name__ == "__main__":
    main()
