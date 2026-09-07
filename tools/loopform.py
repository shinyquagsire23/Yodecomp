#!/usr/bin/env python3
"""LOOP-FORM target list (lesson #40): where does the ORIGINAL use a guarded COUNTDOWN
backedge while OUR source spells an UP-COUNT COMPARE loop — either `for (x = 0; x < n; x++)`
or the guarded `do { ...; i++; } while (i < n);`?

⭐ v129 GENERALISED THE PATTERN, and the thing it could not see was the find. The first
version matched only the `for` spelling, so `LoadWorld` 0x421fd0 — whose delete loop is a
guarded do-while with an up-count backedge — was INVISIBLE to it, and the defect was sitting
in that function's own park note ("delete-loop countdown (dec/jne) vs up-count+spill") for
several sessions. Writing the house countdown there took it **1047 B -> 485 B with the length
landing exactly on the 1684-byte Ghidra extent, +0/-0 collateral**. Same failure family as
v126's `xjumpscan` (a census that hard-codes one instance's SHAPE silently under-reports) —
the tool was right about everything it looked at. Match the MECHANISM, not the spelling.

This is the instrument that found the v113 win (RemoveEmptyZonesFromPlacedList 0x403070,
26 B -> 24 B). It is READ-ONLY: it disassembles YodaDemo.exe and greps src/, and never
compiles or touches build/*.obj — so it is safe to run while a vartest sweep is in flight.

⚠ READ A HIT AS A CANDIDATE, NOT A DEFECT. cl 10.20 strength-reduces most
`for (i = 0; i < n; i++)` loops into a countdown by itself, so the original having a
countdown backedge does NOT mean our source is wrong. What the v113 win showed is that the
SOURCE form can still matter even when the emitted loop is identical: writing the house
guarded countdown changed which register `this` landed in and removed four unrelated
this-relative load diffs. Confirm every hit with `tools/bytediff.py` before investing.

⚠ And LENGTH does not always discriminate here (the v107 shortcut): on 0x403070 both loop
forms emit exactly 206 bytes. Where the length DOES differ, it still refutes instantly.

⭐ Baseline rule (v100/v101/v103): this tool prints a positive control before any finding —
the number of markers/extents parsed and two functions whose answer is already known. An
empty result from a scan is a bug hypothesis, not a finding.

Usage:
    python3 tools/loopform.py            # non-exact residuals only (the actionable list)
    python3 tools/loopform.py --all      # every marker, exact ones included
"""
import re, os, sys, glob, capstone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

EXE = open(os.path.join(ROOT, "YodaDemo/YodaDemo.exe"), "rb").read()
BASE, OFF = 0x401000, 0x400
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

BRANCH = ("jne", "jnz", "jl", "jb", "jle", "jg", "jge", "ja", "jae")
FOR_CMP = re.compile(r"for\s*\(\s*(?:int\s+|short\s+|unsigned\s+\w+\s+)?\w+\s*=\s*0\s*;\s*\w+\s*[<>]")
# The SAME defect wearing the other spelling: a guarded do-while whose backedge COMPARES an
# up-counted index instead of counting a copy of the size down to zero.  `} while (n != 0);`
# (the house countdown) and `} while (nDone == 0);` (a flag) deliberately do NOT match.
DOWHILE_CMP = re.compile(r"\}\s*while\s*\(\s*\w+\s*[<>]")
MARK = re.compile(r"^// FUNCTION: YODA 0x([0-9a-fA-F]{8})", re.M)
LINE_COMMENT = re.compile(r"//[^\n]*")
BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.S)


def strip_comments(text):
    """A loop mentioned in a PARK NOTE is prose, not code — the source notes in this project
    quote loop spellings constantly, and counting them made 0x403070 report 2 `for` loops
    when it has 1."""
    return LINE_COMMENT.sub("", BLOCK_COMMENT.sub("", text))


def extents():
    d = {}
    for line in open(os.path.join(ROOT, "toolchain/test/app_funcs.txt")):
        va, n = line.split()
        d[int(va, 16)] = int(n)
    return d


def bodies():
    """marker VA -> (tu, source text of that function up to the next marker)"""
    out = {}
    for f in sorted(glob.glob(os.path.join(ROOT, "src/*.cpp"))):
        s = open(f).read()
        ms = list(MARK.finditer(s))
        for k, m in enumerate(ms):
            end = ms[k + 1].start() if k + 1 < len(ms) else len(s)
            out[int(m.group(1), 16)] = (os.path.basename(f), s[m.start():end])
    return out


def backedges(va, ext):
    """classify the original's BACKWARD branches as countdown / compare / ?

    Decoded from the function start over its Ghidra extent. A linear sweep can desync on an
    embedded jump table or EH data (the v110 savescan trap), which shows up as '?' entries —
    treat a mostly-'?' row as undecodable rather than as evidence."""
    n = ext.get(va)
    if not n:
        return None
    ins = list(md.disasm(EXE[va - BASE + OFF: va - BASE + OFF + n], va))
    if not ins:
        return None
    kinds = []
    for k, i in enumerate(ins):
        if i.mnemonic not in BRANCH:
            continue
        try:
            tgt = int(i.op_str, 16)
        except ValueError:
            continue
        if tgt >= i.address:          # forward branch — not a backedge
            continue
        prev = [p.mnemonic for p in ins[max(0, k - 3):k]]
        if any(p in ("dec", "sub") for p in prev):
            kinds.append("countdown")
        elif any(p == "cmp" for p in prev):
            kinds.append("compare")
        else:
            kinds.append("?")
    return kinds


def main():
    show_all = "--all" in sys.argv[1:]
    ext, bods = extents(), bodies()

    print("positive control: %d markers parsed, %d extents, e.g. 0x%08x %s"
          % (len(bods), len(ext), sorted(bods)[0], bods[sorted(bods)[0]][0]))
    for va, name, want in ((0x4033b0, "SaveZoneRecursive", "countdown"),
                           (0x403070, "RemoveEmptyZones", "countdown")):
        k = backedges(va, ext)
        ok = k and want in k
        print("  control 0x%08x %-20s backedges=%-34s %s"
              % (va, name, k, "OK" if ok else "!! EXPECTED %s" % want))
        if not ok:
            raise SystemExit("positive control FAILED — fix the scan before trusting it")

    exact = set()
    if not show_all:
        # Read a CACHED exact set so this tool stays read-only (exactset.py compiles, which
        # would fight a vartest sweep over build/*.obj). Produce one with:
        #     python3 tools/exactset.py > /tmp/exact.txt
        path = None
        for k, a in enumerate(sys.argv[1:]):
            if a == "--exact":
                path = sys.argv[k + 2]
        if not path:
            raise SystemExit(
                "give --exact <file> (from `python3 tools/exactset.py > file`) to filter to\n"
                "non-exact residuals, or --all to list every marker.")
        for line in open(path):
            if line.strip():
                exact.add(int(line.split()[0], 16))
        # ⚠ v129: the old line here asserted "0x403070 exact? expected False" — that
        # function became byte-exact at v116, so the control had ROTTEN into a permanent
        # false alarm (the "stale green state" lesson).  Anchor it on a fact that cannot
        # rot instead: the cached set must be non-empty and must not contain everything.
        print("  control exact set = %d from %s (%s)"
              % (len(exact), path,
                 "OK" if 0 < len(exact) < len(bods) + len(exact) else "!! LOOKS WRONG"))
    print()

    rows = []
    for va, (tu, body) in sorted(bods.items()):
        if va in exact:
            continue
        k = backedges(va, ext)
        if not k or "countdown" not in k:
            continue
        clean = strip_comments(body)
        nfor = len(FOR_CMP.findall(clean))
        ndw = len(DOWHILE_CMP.findall(clean))
        if nfor or ndw:
            rows.append((va, tu, k.count("countdown"), k.count("compare"), nfor, ndw))

    print("ORIGINAL has a countdown backedge AND our source spells an UP-COUNT COMPARE loop")
    print("(strongest signal = orig-cmp 0 with our-cmp > 0: the original uses NO compare loop at all)")
    print("(our-dowc = `do { ...; i++; } while (i < n);` — the v129 spelling `for` alone missed)")
    print("%-12s %-22s %-9s %-9s %-8s %s" % ("addr", "tu", "orig-cd", "orig-cmp", "our-for", "our-dowc"))
    for va, tu, cd, cm, nf, nd in sorted(rows, key=lambda r: (r[3], -r[2])):
        print("0x%08x   %-22s %-9d %-9d %-8d %d" % (va, tu, cd, cm, nf, nd))
    print("\n%d candidate(s)%s" % (len(rows), "" if show_all else " among non-exact residuals"))


if __name__ == "__main__":
    main()
