// Find undefined / orphan CODE holes inside a function's address span and
// attach them to the function -- disassembling first if needed -- so the body
// is contiguous and the extent is correct for byte-matching.
//
// WHAT A HOLE IS: MSVC emits a function's C++ EH catch/cleanup blocks in the
// MIDDLE of the function's /Gy COMDAT (reached only via the exception
// dispatcher, so no normal-flow edge). Ghidra therefore leaves them out of the
// function body -- either as raw UNDEFINED bytes or as disassembled-but-ORPHAN
// instructions (func=none) sitting between two body ranges. Example: the
// 41-byte catch block at 0x42905b inside AddItemToInv.
//
// SAFE DETECTION -- a hole is only filled when ALL hold:
//   * it lies within [f.entry, f.bodyMax] but is NOT in f's body,
//   * it is not 0xCC padding (YodaDemo pads with 0xCC only; 0x00 is NOT padding
//     -- it appears inside instructions like `push 0`),
//   * it is not owned by another function,
//   * it TILES cleanly into valid instructions (pseudo-disassembly, each insn
//     fully inside the hole -- a misaligned/overshooting decode = data), and
//   * it ends in RET/JMP or flows straight back into the body.
// Holes larger than MAX_AUTO are REPORTED, not filled: big regions can embed
// jump tables that also happen to decode as valid x86 (e.g. the ~2.4KB switch
// run near Tick/OnTimer that CLAUDE.md warns about), so they need a human look.
//
// DRY_RUN=true reports the plan without mutating. App region only.
//
//@category Yodecomp
//@author Yodecomp

import ghidra.app.script.GhidraScript;
import ghidra.app.util.PseudoDisassembler;
import ghidra.app.util.PseudoInstruction;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;

public class FillFunctionHoles extends GhidraScript {

    private static final long SCAN_START = 0x401000L;
    private static final long SCAN_END   = 0x4292f0L;   // app region (thunks/lib after)
    private static final boolean DRY_RUN = true;
    private static final long MAX_AUTO   = 0x40;        // fill <= 64B; report larger
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

        java.util.LinkedHashMap<Function, AddressSet> plan = new java.util.LinkedHashMap<>();
        int autoN = 0, reviewN = 0;
        long autoBytes = 0;
        StringBuilder log = new StringBuilder();
        StringBuilder review = new StringBuilder();

        FunctionIterator fit = fm.getFunctions(true);
        while (fit.hasNext()) {
            Function f = fit.next();
            long en = f.getEntryPoint().getOffset();
            if (en < SCAN_START || en >= SCAN_END) continue;
            AddressSetView body = f.getBody();
            if (body.getNumAddressRanges() < 2) continue;
            long lo = body.getMinAddress().getOffset();
            long hi = body.getMaxAddress().getOffset();

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
                if (end == null) continue;   // not clean code / doesn't connect
                long sz = re - rs + 1;
                if (sz <= MAX_AUTO) {
                    plan.computeIfAbsent(f, k -> new AddressSet()).add(toAddr(rs), toAddr(re));
                    autoN++; autoBytes += sz;
                    log.append(String.format("FILL %08x-%08x (%dB) %s [%s]\n", rs, re, sz, f.getName(), end));
                } else {
                    reviewN++;
                    review.append(String.format("  REVIEW %08x-%08x (%dB) %s [%s]\n", rs, re, sz, f.getName(), end));
                }
            }
        }

        int filled = 0;
        if (!DRY_RUN) {
            for (java.util.Map.Entry<Function, AddressSet> e : plan.entrySet()) {
                Function f = e.getKey();
                AddressSet add = e.getValue();
                for (ghidra.program.model.address.AddressRange r : add) {
                    if (getInstructionAt(r.getMinAddress()) == null) disassemble(r.getMinAddress());
                }
                try {
                    AddressSet nb = new AddressSet(f.getBody());
                    nb.add(add);
                    f.setBody(nb);
                    filled += add.getNumAddressRanges();
                } catch (Exception ex) {
                    log.append("FAIL " + f.getName() + ": " + ex.getMessage() + "\n");
                }
            }
        }

        println("=== FillFunctionHoles (" + (DRY_RUN ? "DRY-RUN" : "LIVE") + ") region=[0x"
                + Long.toHexString(SCAN_START) + ",0x" + Long.toHexString(SCAN_END) + ") ===");
        println("AUTO holes to attach (<=0x" + Long.toHexString(MAX_AUTO) + "B): " + autoN
                + " (" + autoBytes + " bytes into " + plan.size() + " functions)"
                + (DRY_RUN ? "" : ("; ranges added=" + filled)));
        println(log.toString());
        println("REVIEW holes (>0x" + Long.toHexString(MAX_AUTO) + "B, may embed jump tables): " + reviewN);
        println(review.toString());
    }

    // Returns "RET"/"JMP"/"flow" if [rs,re] tiles into valid instructions that
    // end in a terminal or flow into the body; null otherwise.
    private String classify(long rs, long re, AddressSetView body, long hi) {
        long cur = rs;
        int n = 0;
        String last = "";
        while (cur <= re) {
            PseudoInstruction pi;
            try { pi = pdis.disassemble(toAddr(cur)); } catch (Exception e) { return null; }
            if (pi == null) return null;
            n++;
            last = pi.getMnemonicString();
            long e = cur + pi.getLength() - 1;
            if (e > re) return null;   // overshoots -> misaligned = data
            cur = e + 1;
        }
        if (cur != re + 1 || n == 0) return null;
        boolean term = last.equalsIgnoreCase("RET") || last.equalsIgnoreCase("JMP");
        boolean flowsIn = (re + 1 <= hi) && body.contains(toAddr(re + 1));
        if (term) return last.toUpperCase();
        if (flowsIn) return "flow";
        return null;
    }
}
