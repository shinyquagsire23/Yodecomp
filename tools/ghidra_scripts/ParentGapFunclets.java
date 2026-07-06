// Parent orphan C++ EH funclets / label runs to their owning function so that
// function BOUNDS are correct for byte-matching.
//
// PROBLEM (see CLAUDE.md "COMDAT trim length INCLUDES EH funclets"):
// MSVC 4.2 emits each C++ function as one /Gy COMDAT = [main body][EH funclets]
// [jump tables], contiguously, then padding, then the next function. Ghidra
// computes a function's body by flow from the entry, so the funclets -- reached
// only via the exception dispatcher / jump tables, not normal flow -- are left
// as ORPHAN code in the inter-function gap. The function's Ghidra extent then
// stops at the main body, shorter than the real COMDAT, and slicing
// exe[entry, entry+len) for a byte-compare is garbage for any EH function.
//
// FIX: attach each orphan funclet's address range to its parent function's body
// (Ghidra function bodies may be a non-contiguous AddressSet), so
// getBody().getMaxAddress() reaches the last funclet and the extent matches the
// recompiled COMDAT.
//
// PARENT DETERMINATION (two independent signals, REF preferred):
//   REF: the funclet is referenced (from the .rdata EH tables, surfaced by
//        Ghidra as a DATA ref whose FROM-address sits inside a function body,
//        or a direct code ref) by exactly ONE function -> that is the parent.
//        Sanity: the funclet must lie just after that parent's body (within
//        MAX_FUNCLET_DIST) or inside a hole in its body.
//   ADJ: (user's heuristic) a no-ref run that decodes as parent-frame code --
//        EBP-relative memory operands with NO own `push ebp` prologue -- ending
//        in a tail-merge JMP (into shared cleanup like ~CString / 0x42a030) or a
//        RET is an EH cleanup funclet. Its parent is the nearest preceding REAL
//        function (the frame it runs on); tail-JMP targets are shared helpers,
//        NOT the parent, so they are not used for attribution.
// Anything that resolves to neither (jump-table data, shared `ret N` stubs,
// ambiguous multi-parent refs) is REPORTED and left untouched.
//
// Restricted to the app region by default -- the library region (>=0x429000) is
// linked from real libs, not byte-matched, and is where the only false parent
// signals were observed.
//
// DRY_RUN=true prints the full (funclet -> parent, new extent) mapping without
// mutating. Set false to apply. Body edits are reversible (re-run Ghidra's
// "Fixup Functions" / recompute, or restore from a saved copy).
//
//@category Yodecomp
//@author Yodecomp

import ghidra.app.script.GhidraScript;
import ghidra.app.util.PseudoDisassembler;
import ghidra.app.util.PseudoInstruction;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.address.AddressRangeIterator;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.FlowType;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.SourceType;

public class ParentGapFunclets extends GhidraScript {

    // ---- configuration -----------------------------------------------------
    private static final long SCAN_START = 0x401000L;
    private static final long SCAN_END   = 0x429000L;   // app region only
    private static final boolean DRY_RUN = false;
    private static final boolean USE_ADJACENCY = true;  // enable EBP+tail-JMP heuristic
    private static final long MAX_FUNCLET_DIST = 0x400; // funclet must be within this past parent body
    private static final boolean ADD_LABELS = true;     // label each absorbed funclet <parent>_ehN

    private Memory mem;
    private FunctionManager fm;
    private ReferenceManager rm;
    private PseudoDisassembler pdis;

    private static boolean isPad(int b) { return b == 0xCC || b == 0x00; }

    @Override
    public void run() throws Exception {
        mem = currentProgram.getMemory();
        fm = currentProgram.getFunctionManager();
        rm = currentProgram.getReferenceManager();
        pdis = new PseudoDisassembler(currentProgram);

        // function bodies (union) + sorted entries for adjacency lookup
        AddressSet funcSet = new AddressSet();
        java.util.TreeMap<Long, Function> byEntry = new java.util.TreeMap<>();
        FunctionIterator fit = fm.getFunctions(true);
        while (fit.hasNext()) {
            Function f = fit.next();
            long a = f.getEntryPoint().getOffset();
            if (a < SCAN_START || a >= SCAN_END) continue;
            funcSet.add(f.getBody());
            byEntry.put(a, f);
        }
        AddressSet all = new AddressSet(toAddr(SCAN_START), toAddr(SCAN_END - 1));
        AddressSet gaps = all.subtract(funcSet);

        int nRef = 0, nAdj = 0, nSkipRefWindow = 0, nSkipAmbiguous = 0, nSkipData = 0, nSkipNoParent = 0;
        long absorbedBytes = 0;
        // parent -> ranges to add (batch per function so setBody is called once)
        java.util.LinkedHashMap<Function, AddressSet> plan = new java.util.LinkedHashMap<>();
        StringBuilder log = new StringBuilder();

        AddressRangeIterator rit = gaps.getAddressRanges();
        while (rit.hasNext()) {
            AddressRange r = rit.next();
            long lo = r.getMinAddress().getOffset();
            long hi = r.getMaxAddress().getOffset();
            long cursor = lo;
            long guard = (hi - lo) + 4;                     // hard cap: <= 1 iteration per byte
            while (cursor <= hi) {
                if (--guard < 0) { log.append("ABORT gap loop guard at " + Long.toHexString(cursor) + "\n"); break; }
                // skip padding to next real byte
                while (cursor <= hi && isPad(mem.getByte(toAddr(cursor)) & 0xff)) cursor++;
                if (cursor > hi) break;
                long runStart = cursor;
                long runEnd = walkExtent(runStart, hi);   // end of this contiguous code run
                if (runEnd < runStart) {                   // byte did not decode (jump-table/data)
                    cursor = runStart + 1;                 // ALWAYS make progress -> no infinite loop
                    nSkipData++;
                    continue;
                }
                cursor = runEnd + 1;                        // advance for next run in this gap

                Address rs = toAddr(runStart);

                // ---- REF signal: unique in-body-ref parent ----
                Function refParent = uniqueInBodyRefParent(rs);
                Function parent = null;
                String how = null;
                if (refParent != null) {
                    long pe = refParent.getEntryPoint().getOffset();
                    long pmax = refParent.getBody().getMaxAddress().getOffset();
                    boolean afterOk = runStart > pmax && (runStart - pmax) <= MAX_FUNCLET_DIST;
                    boolean holeOk = runStart >= pe && runStart <= pmax;
                    if (afterOk || holeOk) { parent = refParent; how = "REF"; }
                    else { nSkipRefWindow++;
                        log.append(String.format("SKIP %08x REF-out-of-window parent=%s pmax=%08x\n",
                                runStart, refParent.getName(), pmax));
                    }
                } else if (hasMultipleInBodyRefParents(rs)) {
                    nSkipAmbiguous++;
                    log.append(String.format("SKIP %08x ambiguous-multi-parent\n", runStart));
                }

                // ---- ADJ signal: EBP-frame + tail-merge funclet, no in-body ref ----
                if (parent == null && how == null && USE_ADJACENCY && !hasAnyInBodyRef(rs)) {
                    if (looksLikeFrameFunclet(runStart, runEnd)) {
                        Function adj = nearestRealPreceding(byEntry, runStart);
                        if (adj != null) {
                            long pmax = adj.getBody().getMaxAddress().getOffset();
                            if (runStart > pmax && (runStart - pmax) <= MAX_FUNCLET_DIST) {
                                parent = adj; how = "ADJ";
                            } else { nSkipNoParent++; }
                        } else { nSkipNoParent++; }
                    } else {
                        nSkipData++;   // jump-table / stub / non-funclet
                    }
                }

                if (parent == null) continue;

                if (how.equals("REF")) nRef++; else nAdj++;
                absorbedBytes += (runEnd - runStart + 1);
                plan.computeIfAbsent(parent, k -> new AddressSet()).add(rs, toAddr(runEnd));
                log.append(String.format("PARENT %08x-%08x [%s] -> %s\n", runStart, runEnd, how, parent.getName()));
            }
        }

        // ---- apply ----
        int funcsTouched = 0;
        if (!DRY_RUN) {
            for (java.util.Map.Entry<Function, AddressSet> e : plan.entrySet()) {
                Function f = e.getKey();
                AddressSet add = e.getValue();
                // disassemble each run start so the body is code, then union
                for (AddressRange ar : add) {
                    if (getInstructionAt(ar.getMinAddress()) == null) disassemble(ar.getMinAddress());
                }
                AddressSet nb = new AddressSet(f.getBody());
                nb.add(add);
                try {
                    f.setBody(nb);
                    funcsTouched++;
                    if (ADD_LABELS) {
                        for (AddressRange ar : add) {
                            // address-based suffix -> unique, never collides with
                            // pre-existing <parent>_ehN function symbols
                            String nm = f.getName() + "_eh_" + Long.toHexString(ar.getMinAddress().getOffset());
                            try { createLabel(ar.getMinAddress(), nm, false, SourceType.USER_DEFINED); }
                            catch (Exception le) { /* label is cosmetic; never block the bounds fix */ }
                        }
                    }
                } catch (Exception ex) {
                    log.append("FAIL setBody " + f.getName() + ": " + ex.getMessage() + "\n");
                }
            }
        }

        println("=== ParentGapFunclets (" + (DRY_RUN ? "DRY-RUN" : "LIVE") + ") region=[0x"
                + Long.toHexString(SCAN_START) + ",0x" + Long.toHexString(SCAN_END) + ") ===");
        println("parented via REF: " + nRef + "   via ADJ(EBP+tailJMP): " + nAdj
                + "   (absorbed " + absorbedBytes + " bytes into " + plan.size() + " functions)");
        println("skipped: ref-out-of-window=" + nSkipRefWindow + "  ambiguous=" + nSkipAmbiguous
                + "  data/stub/non-funclet=" + nSkipData + "  no-parent=" + nSkipNoParent);
        if (!DRY_RUN) println("functions whose bounds were extended: " + funcsTouched);
        println(log.toString());
    }

    // Walk a contiguous instruction run from start; stop before padding, a
    // function entry, or an undecodable byte. Returns the last byte offset.
    private long walkExtent(long start, long hardMax) {
        Address cur = toAddr(start);
        long end = start - 1;
        for (int k = 0; k < 512; k++) {
            PseudoInstruction pi;
            try { pi = pdis.disassemble(cur); } catch (Exception e) { break; }
            if (pi == null) break;
            long e = cur.getOffset() + pi.getLength() - 1;
            if (e > hardMax) { end = hardMax; break; }
            end = e;
            long next = e + 1;
            if (next > hardMax) break;
            if (fm.getFunctionAt(toAddr(next)) != null) break;
            int nb;
            try { nb = mem.getByte(toAddr(next)) & 0xff; } catch (Exception ex) { break; }
            if (isPad(nb)) break;
            cur = toAddr(next);
        }
        return end;
    }

    private Function uniqueInBodyRefParent(Address a) {
        Function found = null;
        ReferenceIterator ri = rm.getReferencesTo(a);
        while (ri.hasNext()) {
            Function f = fm.getFunctionContaining(ri.next().getFromAddress());
            if (f == null) continue;
            if (found == null) found = f;
            else if (!found.equals(f)) return null; // multiple distinct parents
        }
        return found;
    }

    private boolean hasMultipleInBodyRefParents(Address a) {
        Function first = null;
        ReferenceIterator ri = rm.getReferencesTo(a);
        while (ri.hasNext()) {
            Function f = fm.getFunctionContaining(ri.next().getFromAddress());
            if (f == null) continue;
            if (first == null) first = f;
            else if (!first.equals(f)) return true;
        }
        return false;
    }

    private boolean hasAnyInBodyRef(Address a) {
        ReferenceIterator ri = rm.getReferencesTo(a);
        while (ri.hasNext()) {
            if (fm.getFunctionContaining(ri.next().getFromAddress()) != null) return true;
        }
        return false;
    }

    // User's heuristic: parent-frame code (EBP-relative memory, no own prologue)
    // terminated by a tail-merge JMP or a RET.
    private boolean looksLikeFrameFunclet(long start, long hardMax) {
        Address cur = toAddr(start);
        boolean sawEbpMem = false, ownPrologue = false, terminated = false;
        for (int k = 0; k < 128; k++) {
            PseudoInstruction pi;
            try { pi = pdis.disassemble(cur); } catch (Exception e) { break; }
            if (pi == null) break;
            String s = pi.toString();
            if (s.startsWith("PUSH EBP")) ownPrologue = true;
            if (s.contains("[EBP")) sawEbpMem = true;
            FlowType ft = pi.getFlowType();
            if (ft.isTerminal()) { terminated = true; break; }        // RET
            if (ft.isJump() && !ft.isConditional()) { terminated = true; break; } // tail JMP
            long next = cur.getOffset() + pi.getLength();
            if (next > hardMax) break;
            if (fm.getFunctionAt(toAddr(next)) != null) break;
            int nb;
            try { nb = mem.getByte(toAddr(next)) & 0xff; } catch (Exception ex) { break; }
            if (isPad(nb)) break;
            cur = toAddr(next);
        }
        return sawEbpMem && !ownPrologue && terminated;
    }

    // Nearest function with entry < addr that is NOT itself a funclet/stub.
    private Function nearestRealPreceding(java.util.TreeMap<Long, Function> byEntry, long addr) {
        java.util.Map.Entry<Long, Function> e = byEntry.lowerEntry(addr);
        while (e != null) {
            Function f = e.getValue();
            String n = f.getName();
            long sz = f.getBody().getNumAddresses();
            boolean funcletish = n.contains("_eh") || sz < 0x10;
            if (!funcletish) return f;
            e = byEntry.lowerEntry(e.getKey());
        }
        return null;
    }
}
