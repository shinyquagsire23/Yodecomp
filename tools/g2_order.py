#!/usr/bin/env python3
"""G2 relative-layout + length-wall analysis (v43).
Two reports over the app TUs (needs build/<TU>.obj compiled + the Ghidra address
cache toolchain/test/orig_func_addrs.txt):
  --walls (default): TRUE length walls — our trimmed COMDAT len (padded to 16) !=
     the ORIGINAL function's true slot (next Ghidra function addr - addr). These are
     the divergences that shift everything downstream. Uses the REAL next-function
     addr (incl. lib COMDATs) so non-marker gaps don't cause false positives.
  --scramble: per-TU emission-ORDER inversions (our obj COMDAT order vs orig addr).
Run from repo root."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match as M

TUS = ['GameTypes','Score','WorldgenHelpers','GameObjects','Iact','Canvas','DeskcppView',
       'IactScript','TextDialog','MainFrm','Deskcpp','DeskcppDoc','Worldgen']
def cpp(tu):
    import glob
    g = glob.glob(f'src/{tu}.cpp')
    return g[0] if g else None

ORIG = sorted(int(x,16) for x in open('toolchain/test/orig_func_addrs.txt'))
def orig_slot(a):
    import bisect
    i = bisect.bisect_right(ORIG, a)
    return (ORIG[i]-a) if i < len(ORIG) else 0

def collect():
    marks=[]   # (addr, tu, name, our_trim_len)
    for tu in TUS:
        obj=f'build/{tu}.obj'; src=cpp(tu)
        if not src or not os.path.exists(obj): continue
        text=open(src).read(); funcs=M.coff_functions(obj)
        for a,n,c,r in M.pair_by_name(text,funcs):
            marks.append((a,tu,n,M.trim_pad(c)))
    return marks

def walls():
    marks=sorted(collect())
    print("=== TRUE LENGTH WALLS (our padded len != orig true slot; shift downstream) ===")
    print("  (only orig-slot 16-aligned funcs shown; non-aligned = EH-funclet/clone-mispair confound, skipped)")
    cum=0; nwall=0; first=None; skipped=0
    for a,tu,n,ol in marks:
        os_=orig_slot(a)
        if os_<=0: continue
        if os_ % 16 != 0:            # funclet split or Ghidra sub-function boundary — unreliable
            skipped+=1; continue
        d=((ol+15)&~15) - os_
        if d!=0:
            nwall+=1; cum+=d
            if first is None: first=(a,tu,n)
            print(f"  {a:x} {tu:9} our={ol:4} origslot={os_:4} d={d:+#5x} cum={cum:+#6x}  {n[:40]}")
    if first:
        print(f"\n  {nwall} clean length walls ({skipped} funclet-confounded skipped). First: 0x{first[0]:x} {first[1]} {first[2][:40]}")
        print(f"  ⇒ absolute LAYOUT is achievable only for the {sum(1 for a,t,n,l in marks if a<first[0])} markers before it.")

def scramble():
    for tu in TUS:
        obj=f'build/{tu}.obj'; src=cpp(tu)
        if not src or not os.path.exists(obj): continue
        text=open(src).read(); funcs=M.coff_functions(obj)
        n2a={n:a for a,n,c,r in M.pair_by_name(text,funcs)}
        seq=[(n2a[n],n) for (n,c,r) in funcs if n in n2a]
        invs=[]; prev=-1; pn=None
        for a,n in seq:
            if a<prev: invs.append((pn,prev,n,a))
            prev,pn=a,n
        tag=f"{len(invs)} inversion(s)" if invs else "in-order"
        print(f"{'===' if invs else '---'} {tu}: {tag}")
        for p,pa,n,a in invs:
            print(f"   after {pa:x} {p[:40]} -> {a:x} {n[:40]}")

if __name__=='__main__':
    if '--scramble' in sys.argv: scramble()
    else: walls()
