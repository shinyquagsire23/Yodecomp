// Find CODE holes in/around a function and turn them into proper instructions
// owned by the function, so the body is contiguous and the extent is correct
// for byte-matching. Two hole kinds, both from MSVC emitting C++ EH catch/
// cleanup blocks with no normal-flow edge:
//
//   A) BETWEEN-RANGE holes: undefined or disassembled-but-orphan code sitting in
//      a gap between two of the function's body ranges (e.g. the 41B catch block
//      at 0x42905b in AddItemToInv). Disassemble + union into the body.
//   B) IN-BODY undefined runs: undefined bytes that are already inside the body
//      AddressSet but were never disassembled -- a run of back-to-back
//      destructor funclets where a caller only disassembled the first one (each
//      funclet ends in `jmp`, so plain disassemble(start) stops after the first
//      and leaves the rest undefined, e.g. the funclets at 0x404359 in Ctor).
//      Just disassemble them fully; they are already in the body.
//
// Both use disassembleRangeFully() -- iterate instruction-by-instruction so a
// run of non-fall-through funclets is fully decoded, not just the first.
//
// SAFETY: a hole is only touched when it TILES cleanly into valid instructions
// (pseudo-disassembly; a misaligned/overshooting decode = data), ends in RET/JMP
// or flows into the body, is not 0xCC padding (0x00 is NOT padding here -- it is
// inside instructions like `push 0`), is not owned by another function, and (for
// kind A) is <= MAX_AUTO. Larger holes are REPORTED, not filled: big regions can
// embed jump tables that also decode as valid x86 (the ~2.4KB switch run near
// Tick/OnTimer). DRY_RUN=true reports without mutating. App region only.
//
//@category Yodecomp
//@author Yodecomp

import ghidra.app.script.GhidraScript;
import ghidra.app.util.PseudoDisassembler;
import ghidra.app.util.PseudoInstruction;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;

public class FillFunctionHoles extends GhidraScript {

    private static final long SCAN_START = 0x401000L;
    private static final long SCAN_END   = 0x4292f0L;
    private static final boolean DRY_RUN = true;
    private static final long MAX_AUTO   = 0x40;   // between-range fill cap
    private static final long MIN_HOLE   = 4;

    private Memory mem;
    private Listing listing;
    private FunctionManager fm;
    private PseudoDisassembler pdis;

    @Override
    public void run() throws Exception {
        mem = currentProgram.getMemory();
        listing = currentProgram.getListing();
        fm = currentProgram.getFunctionManager();
        pdis = new PseudoDisassembler(currentProgram);

        // plan A: function -> ranges to disassemble AND union into the body
        java.util.LinkedHashMap<Function, AddressSet> unionPlan = new java.util.LinkedHashMap<>();
        // plan B: ranges already in a body, just disassemble
        java.util.ArrayList<long[]> disPlan = new java.util.ArrayList<>();
        int aN = 0, bN = 0, reviewN = 0;
        long aBytes = 0, bBytes = 0;
        StringBuilder log = new StringBuilder(), review = new StringBuilder();

        FunctionIterator fit = fm.getFunctions(true);
        while (fit.hasNext()) {
            Function f = fit.next();
            long en = f.getEntryPoint().getOffset();
            if (en < SCAN_START || en >= SCAN_END) continue;
            AddressSetView body = f.getBody();
            long lo = body.getMinAddress().getOffset();
            long hi = body.getMaxAddress().getOffset();

            // --- kind A: between-range holes ---
            if (body.getNumAddressRanges() >= 2) {
                long a = lo;
                while (a <= hi) {
                    if (body.contains(toAddr(a)) || (mem.getByte(toAddr(a)) & 0xff) == 0xCC
                            || fm.getFunctionContaining(toAddr(a)) != null) { a++; continue; }
                    long rs = a;
                    while (a <= hi && !body.contains(toAddr(a)) && (mem.getByte(toAddr(a)) & 0xff) != 0xCC
                            && fm.getFunctionContaining(toAddr(a)) == null) a++;
                    long re = a - 1;
                    if (re - rs + 1 < MIN_HOLE) continue;
                    String end = classify(rs, re, body, hi);
                    if (end == null) continue;
                    long sz = re - rs + 1;
                    if (sz <= MAX_AUTO) {
                        unionPlan.computeIfAbsent(f, k -> new AddressSet()).add(toAddr(rs), toAddr(re));
                        aN++; aBytes += sz;
                        log.append(String.format("FILL(A) %08x-%08x (%dB) %s [%s]\n", rs, re, sz, f.getName(), end));
                    } else {
                        reviewN++;
                        review.append(String.format("  REVIEW %08x-%08x (%dB) %s [%s]\n", rs, re, sz, f.getName(), end));
                    }
                }
            }

            // --- kind B: undefined runs already inside the body ---
            for (AddressRange r : body.getAddressRanges()) {
                long a = r.getMinAddress().getOffset(), rhi = r.getMaxAddress().getOffset();
                while (a <= rhi) {
                    if (listing.getInstructionContaining(toAddr(a)) != null
                            || listing.getDefinedDataContaining(toAddr(a)) != null) { a++; continue; }
                    long rs = a;
                    while (a <= rhi && listing.getInstructionContaining(toAddr(a)) == null
                            && listing.getDefinedDataContaining(toAddr(a)) == null) a++;
                    long re = a - 1;
                    if (tiles(rs, re) == 0) continue;   // not clean code -> leave
                    disPlan.add(new long[]{rs, re});
                    bN++; bBytes += (re - rs + 1);
                    log.append(String.format("DISASM(B) %08x-%08x (%dB) %s\n", rs, re, re - rs + 1, f.getName()));
                }
            }
        }

        int filledA = 0, disB = 0;
        if (!DRY_RUN) {
            for (java.util.Map.Entry<Function, AddressSet> e : unionPlan.entrySet()) {
                Function f = e.getKey();
                AddressSet add = e.getValue();
                for (AddressRange r : add) disassembleRangeFully(r.getMinAddress().getOffset(), r.getMaxAddress().getOffset());
                try {
                    AddressSet nb = new AddressSet(f.getBody());
                    nb.add(add);
                    f.setBody(nb);
                    filledA += add.getNumAddressRanges();
                } catch (Exception ex) { log.append("FAIL " + f.getName() + ": " + ex.getMessage() + "\n"); }
            }
            for (long[] pr : disPlan) if (disassembleRangeFully(pr[0], pr[1])) disB++;
        }

        println("=== FillFunctionHoles (" + (DRY_RUN ? "DRY-RUN" : "LIVE") + ") region=[0x"
                + Long.toHexString(SCAN_START) + ",0x" + Long.toHexString(SCAN_END) + ") ===");
        println("A: between-range holes filled (<=0x" + Long.toHexString(MAX_AUTO) + "B): " + aN + " (" + aBytes + " B)"
                + (DRY_RUN ? "" : ("; ranges added=" + filledA)));
        println("B: in-body undefined code runs disassembled: " + bN + " (" + bBytes + " B)"
                + (DRY_RUN ? "" : ("; done=" + disB)));
        println(log.toString());
        println("REVIEW holes (>0x" + Long.toHexString(MAX_AUTO) + "B, may embed jump tables): " + reviewN);
        println(review.toString());
    }

    // Disassemble every instruction in [start,end], stepping instruction by
    // instruction so a run of `jmp`-terminated funclets is fully decoded (plain
    // disassemble(start) stops after the first funclet). Returns true if covered.
    private boolean disassembleRangeFully(long start, long end) {
        long cur = start;
        int guard = 0;
        while (cur <= end && guard++ < 256) {
            if (getInstructionAt(toAddr(cur)) == null) disassemble(toAddr(cur));
            Instruction ins = getInstructionAt(toAddr(cur));
            if (ins == null) return false;
            cur += ins.getLength();
        }
        return cur > end;
    }

    // classify a between-range hole; returns RET/JMP/flow or null. Rejects any
    // run that establishes its own frame (`mov eax,fs:[0]` SEH setup or
    // `push ebp`) -- that is a SEPARATE function whose (often broken/tiny) body
    // left it sitting in the gap, NOT a catch block. Filling it would swallow
    // another function (the OnEraseBkgnd-inside-OnMouseMove false positive).
    private String classify(long rs, long re, AddressSetView body, long hi) {
        if (tiles(rs, re) == 0) return null;
        long cur = rs; String last = "";
        while (cur <= re) { PseudoInstruction pi;
            try { pi = pdis.disassemble(toAddr(cur)); } catch (Exception e) { return null; }
            if (pi == null) return null;
            String s = pi.toString().toUpperCase();
            if (s.startsWith("MOV EAX,FS:[") || s.equals("PUSH EBP")) return null; // separate function
            last = pi.getMnemonicString(); cur += pi.getLength(); }
        boolean term = last.equalsIgnoreCase("RET") || last.equalsIgnoreCase("JMP");
        if (term) return last.toUpperCase();
        if ((re + 1 <= hi) && body.contains(toAddr(re + 1))) return "flow";
        return null;
    }

    // 1 if [rs,re] tiles exactly into valid instructions, else 0
    private int tiles(long rs, long re) {
        long cur = rs; int n = 0;
        while (cur <= re) { PseudoInstruction pi;
            try { pi = pdis.disassemble(toAddr(cur)); } catch (Exception e) { return 0; }
            if (pi == null) return 0;
            long e = cur + pi.getLength() - 1; if (e > re) return 0;
            cur = e + 1; n++;
        }
        return (cur == re + 1 && n > 0) ? 1 : 0;
    }
}
