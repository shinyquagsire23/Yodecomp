#!/usr/bin/env python3
"""First-pass compile-unit (.obj) segmentation of YodaDemo.exe's app region.

Signal: MSVC emits each translation unit's .rdata/.data (strings, FP constants,
vtables, globals) as a contiguous block, concatenated by the linker in the SAME
order as .text. So a function's *lowest* referenced data address ("lo") advances
monotonically with its .text address, stepping up at each .obj boundary. Functions
with no data refs (the majority) inherit the enclosing segment by contiguity.

Input: toolchain/test/cu_refs.txt  (lines: "entry lo hi cnt", hex, from Ghidra)
Output: a segmentation printed to stdout.
"""
import sys

SHARED = {0x44ccc4, 0x44cfd4, 0x44cfec}  # base vtables / RTTI referenced from everywhere

rows = []
for ln in open(sys.argv[1] if len(sys.argv) > 1 else "toolchain/test/cu_refs.txt"):
    p = ln.split()
    if len(p) != 4:
        continue
    entry, lo, hi, cnt = int(p[0], 16), int(p[1], 16), int(p[2], 16), int(p[3])
    rows.append((entry, lo, hi, cnt))
rows.sort()

# anchors = functions with a data ref whose lo isn't purely a shared vtable
anchors = []
for entry, lo, hi, cnt in rows:
    if cnt == 0:
        continue
    if lo in SHARED:            # references only a shared base vtable -> not an .obj signal
        continue
    anchors.append((entry, lo, hi))

# Walk anchors; a boundary is where lo jumps forward past the running high-water
# mark of the current segment's data block (a genuinely new .rdata/.data cluster).
# Small backward refs within a segment are normal (a late function reusing an early
# string), so we only cut on a FORWARD jump beyond the segment's data hi.
GAP = 0x20  # min forward jump (bytes) in lo to call it a new data block
segments = []              # (text_start, text_end, data_lo, data_hi, n_anchors)
seg_text0 = anchors[0][0]
seg_dlo = anchors[0][1]
seg_dhi = anchors[0][2]
seg_n = 1
for i in range(1, len(anchors)):
    entry, lo, hi = anchors[i]
    if lo > seg_dhi + GAP:      # this function's data starts beyond current block -> boundary
        segments.append((seg_text0, entry, seg_dlo, seg_dhi, seg_n))
        seg_text0 = entry
        seg_dlo = lo
        seg_dhi = hi
        seg_n = 1
    else:
        seg_dhi = max(seg_dhi, hi)
        seg_dlo = min(seg_dlo, lo)
        seg_n += 1
segments.append((seg_text0, 0x429000, seg_dlo, seg_dhi, seg_n))

# count total funcs (incl. no-ref) per segment via contiguity
allfuncs = [r[0] for r in rows]
def nfuncs(a, b):
    return sum(1 for f in allfuncs if a <= f < b)

print("%-3s %-19s %-17s %-6s %s" % ("#", ".text range", ".data cluster", "funcs", "anchors"))
for i, (t0, t1, dlo, dhi, n) in enumerate(segments):
    print("%-3d %06x-%06x     %06x-%06x   %-6d %d" % (i, t0, t1, dlo, dhi, nfuncs(t0, t1), n))
print("\nsegments=%d  total app funcs=%d" % (len(segments), len(allfuncs)))
