#!/usr/bin/env python3
"""Dump the byte-exact function set (addr + mangled name) for the current compiler.
Honors the VCDIR env override (via toolchain/bin/cl) so you can A/B compiler builds:
  python3 tools/exactset.py > /tmp/e42.txt                 # default VC 4.2
  VCDIR=$PWD/toolchain/vc40 python3 tools/exactset.py > /tmp/e40.txt
  comm -13 <(sort /tmp/e42.txt) <(sort /tmp/e40.txt)      # funcs the candidate wins
Reuses match/verify like progress.py; recompiles every src/**/*.cpp into build/."""
import os,sys,glob,subprocess
ROOT="/Users/maxamillion/workspace/Yodecomp"
sys.path.insert(0, os.path.join(ROOT,"tools"))
import match, verify
EXE=open(os.path.join(ROOT,"YodaDemo","YodaDemo.exe"),"rb").read()
CL=os.path.join(ROOT,"toolchain","bin","cl")
build=os.path.join(ROOT,"build")
flags="/nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS".split()
def compile_obj(cpp):
    obj=os.path.join(build,os.path.splitext(os.path.basename(cpp))[0]+".obj")
    try: os.remove(obj)
    except: pass
    fo="/Fo"+os.path.relpath(obj,os.path.dirname(cpp))
    r=subprocess.run([CL]+flags+[fo,os.path.basename(cpp)],cwd=os.path.dirname(cpp),
                     env=os.environ,capture_output=True)
    return obj if os.path.exists(obj) else None
exact=set()
for cpp in sorted(glob.glob(os.path.join(ROOT,"src","**","*.cpp"),recursive=True)):
    obj=compile_obj(cpp)
    if not obj: continue
    text=open(cpp).read()
    funcs=[f for f in match.coff_functions(obj)
           if verify.owner_of(f[0]) not in verify.LIB_OWNERS
           and not f[0].lstrip("?").startswith(("_$E","$E"))]
    for va,name,code,relocs in match.pair_by_name(text,funcs):
        L=match.trim_pad(code); foff=(va-match.TEXT_VA)+match.TEXT_RAW
        orig=EXE[foff:foff+L]
        cm,om=match.mask(code,relocs,L),match.mask(orig,relocs,L)
        d=sum(1 for i in range(min(len(cm),len(om))) if cm[i]!=om[i])
        if d==0 and len(orig)==L: exact.add((va,name))
for va,name in sorted(exact):
    print("%#010x %s"%(va,name))
