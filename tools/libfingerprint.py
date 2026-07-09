#!/usr/bin/env python3
"""Which VC release's static libs were linked into YodaDemo.exe?

The statically-linked CRT (LIBCMT.LIB) + MFC (NAFXCW.LIB) code in the exe's
library region is MS-PREBUILT and version-specific, so byte-matching it against
each candidate VC's libs pins the toolchain version INDEPENDENTLY of the app-code
compiler analysis (progress.py / asmscore.py). Slides reloc-tolerant windows over
the library region and counts, per version, how many appear verbatim in that
version's libs -- and how many are UNIQUE to it (the discriminating fingerprint).
Reloc-bearing windows match nothing, so they don't bias the vote.

v52 result: 1404 windows unique to vc42, 0 to vc40/vc41 => libraries = VC 4.2.

Usage:  python3 tools/libfingerprint.py [LIBREG_START LIBREG_END]
        (defaults 0x429000 0x44b000 -- the static-library run of YodaDemo.exe)
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__))))
import match

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = open(os.path.join(ROOT, "YodaDemo", "YodaDemo.exe"), "rb").read()
VERS = ("40", "41", "42")           # toolchain/vc<VER>/... must exist
W, STEP = 20, 8

def off(va): return va - match.TEXT_VA + match.TEXT_RAW

def main():
    start = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0x429000
    end   = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x44b000
    region = EXE[off(start):off(end)]
    libs = {}
    for v in VERS:
        crt = open(os.path.join(ROOT, "toolchain", "vc%s" % v, "LIB", "LIBCMT.LIB"), "rb").read()
        mfc = open(os.path.join(ROOT, "toolchain", "vc%s" % v, "MFC", "LIB", "NAFXCW.LIB"), "rb").read()
        libs[v] = (crt, mfc)
    hit = {v: 0 for v in VERS}; uniq = {v: 0 for v in VERS}; tot = 0
    for i in range(0, len(region) - W, STEP):
        w = region[i:i+W]
        if w.count(0) > W // 2:      # skip padding / zero-heavy windows
            continue
        tot += 1
        present = [v for v in VERS if any(w in b for b in libs[v])]
        for v in present:
            hit[v] += 1
        if len(present) == 1:
            uniq[present[0]] += 1
    print("lib region %#x-%#x (%d B); %d non-pad windows (W=%d step=%d)"
          % (start, end, len(region), tot, W, STEP))
    print("  ver | any-lib hits | UNIQUE-to-ver")
    for v in VERS:
        print("  vc%s | %11d | %d" % (v, hit[v], uniq[v]))
    win = max(VERS, key=lambda v: uniq[v])
    print("  => linked libraries fingerprint to VC %s.%s (unique=%d)"
          % (win[0], win[1], uniq[win]))

if __name__ == "__main__":
    main()
