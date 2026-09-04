#!/usr/bin/env python3
"""READ-ONLY target list for the UNROTATED-LOOP dial (v114, lesson #46).

⭐ What it looks for. cl 10.20 normally ROTATES a `while`/`for` loop into
"entry guard + body + duplicated bottom test". For `for (;;) { if (exit) break; ... }`
it does NOT: it emits the test at the loop TOP and an unconditional `jmp` BACK to that
test. That back-`jmp` is the fingerprint, and it is LENGTH-visible — the rotated form is
4+ bytes longer at the back edge, so the two forms can be told apart from the original's
extent alone. This is what took WorldgenShuffleList 0x41ef90 from 269 B to 22 B, where
Ghidra's extent (396 B) matched the unrotated spelling exactly and refuted the `while`
spelling (400 B).

⚠ This is a SIBLING of loopform.py, not a replacement: loopform.py asks "countdown vs
compare backedge" (lesson #40); this asks "rotated vs unrotated". Both are read-only —
no compile, no build/*.obj — so either is safe to run while a vartest.py sweep is in
flight. Pass a cached exact-set file from `tools/exactset.py`.

⚠ Positive control (v100/v101 baseline rule): asserts 0x41ef90 reports its 2 unrotated
loops and that a known countdown function reports 0. The control has already earned its
keep once — it caught an off-by-one in the very first version of the detector.

⚠ A relaxed variant of this scan (accepting any conditional branch within 4 instructions
of the back-jmp target, rather than requiring a `cmp`/`test` there) adds exactly one hit,
0x40fca0 — which has NO loops at all in its source. That is an embedded switch JUMP TABLE
being decoded as instructions, the same trap that produced v100's "8 fake class-D targets".
Keep the strict rule.

Usage:
    python3 tools/unrotscan.py [--exact <file from exactset.py>] [--all]
"""
import sys, os, re, subprocess, capstone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = open(os.path.join(ROOT, "YodaDemo/YodaDemo.exe"), "rb").read()
TEXT_VA, TEXT_RAW = 0x401000, 0x400
MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def extents():
    d = {}
    for line in open(os.path.join(ROOT, "toolchain/test/app_funcs.txt")):
        p = line.split()
        if len(p) >= 2:
            d[int(p[0], 16)] = int(p[1])
    return d


def markers():
    d = {}
    for tu in subprocess.check_output(["git", "ls-files", "src/*.cpp"], cwd=ROOT).decode().split():
        txt = subprocess.check_output(["git", "show", "HEAD:" + tu], cwd=ROOT).decode("utf-8", "replace")
        for m in re.finditer(r"// FUNCTION: YODA (0x[0-9a-fA-F]+)", txt):
            d[int(m.group(1), 16)] = tu
    return d


def unrotated(va, n):
    """Count back-edges that are an unconditional jmp to a cmp/test whose conditional
    branch exits FORWARD past that jmp — i.e. the loop-exit test sits at the loop TOP."""
    off = va - TEXT_VA + TEXT_RAW
    ins = list(MD.disasm(EXE[off:off + n], va))
    by = {i.address: i for i in ins}
    hits = 0
    for i in ins:
        if i.mnemonic != "jmp" or not i.op_str.startswith("0x"):
            continue
        tgt = int(i.op_str, 16)
        if not (va <= tgt < i.address):
            continue
        t = by.get(tgt)
        if t is None or t.mnemonic not in ("cmp", "test"):
            continue
        nxt = by.get(tgt + t.size)
        if nxt is None or nxt.mnemonic == "jmp" or not nxt.mnemonic.startswith("j"):
            continue
        if nxt.op_str.startswith("0x") and int(nxt.op_str, 16) > i.address:
            hits += 1
    return hits


def main():
    args = sys.argv[1:]
    ext, mk = extents(), markers()
    exact = set()
    if "--exact" in args:
        for line in open(args[args.index("--exact") + 1]):
            exact.add(int(line.split()[0], 16))
    show_all = "--all" in args

    assert unrotated(0x41ef90, ext[0x41ef90]) == 2, "positive control failed"
    assert unrotated(0x4033b0, ext[0x4033b0]) == 0, "negative control failed"
    print("positive control: %d markers, %d extents; 0x41ef90 has 2 unrotated loops, "
          "0x4033b0 (countdown) has 0" % (len(mk), len(ext)))

    rows = []
    for va, tu in sorted(mk.items()):
        if va not in ext or (va in exact and not show_all):
            continue
        h = unrotated(va, ext[va])
        if h:
            rows.append((h, ext[va], va, tu))

    print("\nORIGINAL contains an UNROTATED loop (test at top, unconditional jmp back)")
    print("%-12s %-24s %6s  %s" % ("addr", "tu", "len", "unrotated loops"))
    for h, n, va, tu in sorted(rows, key=lambda r: (-r[0], -r[1])):
        print("0x%08x   %-24s %6d  %d" % (va, tu.replace("src/", ""), n, h))
    print("\n%d candidate(s)%s" % (len(rows), "" if show_all else " among non-exact residuals"))


if __name__ == "__main__":
    main()
