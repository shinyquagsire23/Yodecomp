// Merge pre-existing EH funclet FUNCTIONS into their parent so function bounds
// are correct for byte-matching. Companion to ParentGapFunclets.java: that one
// absorbs ORPHAN gap funclets (never made into functions); this one folds in the
// funclets a prior pass DID turn into standalone functions (typically named
// `<parent>_ehN`), which still split the parent's extent with a hole.
//
// A funclet is NOT a real function -- it is C++ EH cleanup code that runs on the
// parent's frame and is part of the parent's /Gy COMDAT. For a correct
// [entry, entry+len) byte-compare the parent must own the funclet's bytes.
//
// FUNCLET vs REAL FUNCTION -- the discriminator is the FRAME, not the call graph.
// MSVC destructor funclets ARE reached by a direct CALL from the parent (normal-
// path cleanup) as well as from the EH tables, so "never called" is FALSE. What
// makes them funclets:
//   * they NEVER establish their own frame -- no `push ebp; mov ebp,esp` (real
//     SEH functions do this at insn 2-3, right after `mov eax,fs:[0]`), and
//   * they address the PARENT's frame directly (`lea ecx,[ebp-X]`, `mov ecx,
//     [ebp-X]`) or are the `mov eax,imm; jmp <unwind handler>` state shape.
// A real single-caller function (e.g. CalcCompletionScore) is either framed or a
// register-only leaf that never touches [ebp] -> correctly left alone.
//
// PARENT: the unique function that references the funclet from inside its body
// (the caller / EH-table site), or -- for `_ehN`-named funclets with no in-body
// ref -- the nearest REAL preceding function. A window guard rejects far matches;
// a size cap is a belt-and-suspenders backstop against mis-classification.
//
// Each merge: capture the funclet body, delete the funclet function, union the
// range into the parent, and leave a label. DRY_RUN=true reports without
// mutating. App region only (library >=0x429000 is linked from real libs).
//
//@category Yodecomp
//@author Yodecomp

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.SourceType;

public class MergeEhFunclets extends GhidraScript {

    private static final long SCAN_START = 0x401000L;
    private static final long SCAN_END   = 0x429000L;   // app region only
    private static final boolean DRY_RUN = false;
    private static final long MAX_FUNCLET_DIST = 0x400;
    private static final long SIZE_CAP = 0x200;          // funclets are tiny; backstop
    private static final boolean KEEP_LABEL = true;
    // Default SAFE: only merge `<parent>_ehN`-named funclets (prior-vetted). A
    // human-assigned descriptive name (PositionMaybe, OnLoadWorld, ...) means a
    // real function -> never merged. Set true to ALSO fold in auto-named
    // (FUN_*/case*/LAB_*) frameless funclets -- reported either way for review.
    private static final boolean MERGE_UNNAMED_FUNCLETS = false;

    private FunctionManager fm;
    private ReferenceManager rm;
    private Listing listing;
    private java.util.TreeMap<Long, Function> byEntry;

    @Override
    public void run() throws Exception {
        fm = currentProgram.getFunctionManager();
        rm = currentProgram.getReferenceManager();
        listing = currentProgram.getListing();

        byEntry = new java.util.TreeMap<>();
        java.util.ArrayList<Function> allFuncs = new java.util.ArrayList<>();
        FunctionIterator fit = fm.getFunctions(true);
        while (fit.hasNext()) {
            Function f = fit.next();
            long a = f.getEntryPoint().getOffset();
            if (a < SCAN_START || a >= SCAN_END) continue;
            byEntry.put(a, f);
            allFuncs.add(f);
        }

        int nRef = 0, nName = 0, nSkipFramed = 0, nSkipNotFunclet = 0, nSkipNamed = 0,
            nSkipNoParent = 0, nSkipWindow = 0, nSkipParentFunclet = 0, nSkipSize = 0;
        int nReviewUnnamed = 0;
        long mergedBytes = 0;
        StringBuilder log = new StringBuilder();
        StringBuilder review = new StringBuilder();
        java.util.LinkedHashMap<Long, Object[]> plan = new java.util.LinkedHashMap<>();

        for (Function f : allFuncs) {
            Address fe = f.getEntryPoint();
            long fa = fe.getOffset();
            String nm = f.getName();
            boolean ehNamed = nm.contains("_eh");
            boolean autoNamed = nm.startsWith("FUN_") || nm.startsWith("case") || nm.startsWith("LAB_");

            // real function establishes its own frame -> not a funclet
            if (establishesOwnFrame(fe)) { nSkipFramed++; continue; }
            // frameless but not a funclet (register-only leaf / ret stub) -> leave
            if (!(ehNamed || referencesParentFrame(f) || isStateFunclet(fe))) { nSkipNotFunclet++; continue; }
            // a human-assigned descriptive name = a real function -> never merge.
            // Auto-named (FUN_*/case*) frameless funclets are reported for review
            // and only merged when MERGE_UNNAMED_FUNCLETS is enabled.
            if (!ehNamed) {
                if (!autoNamed) { nSkipNamed++; continue; }
                if (!MERGE_UNNAMED_FUNCLETS) {
                    nReviewUnnamed++;
                    if (nReviewUnnamed <= 60) review.append(String.format("  REVIEW %08x %s (size=%d)\n", fa, nm, f.getBody().getNumAddresses()));
                    continue;
                }
            }
            if (f.getBody().getNumAddresses() > SIZE_CAP) {
                nSkipSize++;
                log.append(String.format("SKIP %08x %s size=%d > cap\n", fa, nm, f.getBody().getNumAddresses()));
                continue;
            }

            Function parent = uniqueInBodyRefParent(fe, f);
            String how;
            if (parent != null && !parent.getName().contains("_eh")) {
                how = "REF";
            } else {
                parent = ehNamed ? nearestRealPreceding(fa) : null;
                how = "NAME";
                if (parent == null) { nSkipNoParent++; continue; }
            }
            long pe = parent.getEntryPoint().getOffset();
            long pmax = parent.getBody().getMaxAddress().getOffset();
            boolean afterOk = fa > pmax && (fa - pmax) <= MAX_FUNCLET_DIST;
            boolean holeOk = fa >= pe && fa <= pmax;
            if (!(afterOk || holeOk)) { nSkipWindow++;
                log.append(String.format("SKIP %08x %s window parent=%s pmax=%08x\n", fa, nm, parent.getName(), pmax));
                continue;
            }
            if (parent.getName().contains("_eh")) { nSkipParentFunclet++; continue; }

            plan.put(fa, new Object[]{ parent, new AddressSet(f.getBody()), how, nm });
            mergedBytes += f.getBody().getNumAddresses();
            if ("REF".equals(how)) nRef++; else nName++;
            log.append(String.format("MERGE %08x %-28s [%s] -> %s\n", fa, nm, how, parent.getName()));
        }

        int done = 0, failed = 0;
        if (!DRY_RUN) {
            for (java.util.Map.Entry<Long, Object[]> e : plan.entrySet()) {
                Address fe = toAddr(e.getKey());
                Object[] v = e.getValue();
                Function parent = (Function) v[0];
                AddressSet body = (AddressSet) v[1];
                String oldName = (String) v[3];
                Function f = fm.getFunctionAt(fe);
                if (f == null) { failed++; continue; }
                try {
                    removeFunction(f);
                    AddressSet nb = new AddressSet(parent.getBody());
                    nb.add(body);
                    parent.setBody(nb);
                    if (KEEP_LABEL) {
                        try { createLabel(fe, oldName, false, SourceType.USER_DEFINED); }
                        catch (Exception le) { /* cosmetic */ }
                    }
                    done++;
                } catch (Exception ex) {
                    failed++;
                    log.append("FAIL merge " + Long.toHexString(e.getKey()) + ": " + ex.getMessage() + "\n");
                }
            }
        }

        println("=== MergeEhFunclets (" + (DRY_RUN ? "DRY-RUN" : "LIVE") + ") region=[0x"
                + Long.toHexString(SCAN_START) + ",0x" + Long.toHexString(SCAN_END) + ") ===");
        println("mergeable funclet functions: via REF=" + nRef + "  via NAME/adj=" + nName
                + "   (" + mergedBytes + " bytes into their parents)");
        println("skipped: framed-real-func=" + nSkipFramed + "  frameless-non-funclet=" + nSkipNotFunclet
                + "  descriptive-name(real)=" + nSkipNamed + "  no-parent=" + nSkipNoParent
                + "  out-of-window=" + nSkipWindow + "  over-size-cap=" + nSkipSize
                + "  parent-is-funclet=" + nSkipParentFunclet);
        println("auto-named (FUN_*/case*) frameless funclets held for REVIEW (not merged): " + nReviewUnnamed
                + (MERGE_UNNAMED_FUNCLETS ? " [MERGE_UNNAMED_FUNCLETS on -> merged above]" : ""));
        if (!DRY_RUN) println("merged=" + done + "  failed=" + failed);
        println(log.toString());
        if (nReviewUnnamed > 0 && !MERGE_UNNAMED_FUNCLETS) { println("--- review candidates ---"); println(review.toString()); }
    }

    // Real function: within the first ~5 instructions it does `push ebp`
    // (MSVC frame prologue, possibly after `mov eax,fs:[0]` for SEH).
    private boolean establishesOwnFrame(Address entry) {
        Instruction i = listing.getInstructionAt(entry);
        for (int k = 0; k < 8 && i != null; k++) {
            if (i.getMnemonicString().equalsIgnoreCase("PUSH")
                    && i.toString().toUpperCase().contains("EBP")) return true;
            i = i.getNext();
        }
        return false;
    }

    // Funclet tell: addresses the (inherited) parent frame via [EBP...].
    private boolean referencesParentFrame(Function f) {
        Instruction i = listing.getInstructionAt(f.getEntryPoint());
        AddressSetView body = f.getBody();
        for (int k = 0; k < 64 && i != null; k++) {
            if (!body.contains(i.getAddress())) break;
            if (i.toString().toUpperCase().contains("[EBP")) return true;
            i = i.getNext();
        }
        return false;
    }

    // State-unwind funclet shape: `mov eax,imm32; jmp <handler>`.
    private boolean isStateFunclet(Address entry) {
        Instruction i = listing.getInstructionAt(entry);
        return i != null && i.getMnemonicString().equalsIgnoreCase("MOV")
                && i.toString().toUpperCase().startsWith("MOV EAX,0X");
    }

    // Unique function (other than self) that references addr from inside its body,
    // by ANY reference type (call for normal-path cleanup, data for the EH table,
    // jump for a switch-case block) -- all point at the owning parent.
    private Function uniqueInBodyRefParent(Address addr, Function self) {
        Function found = null;
        ReferenceIterator ri = rm.getReferencesTo(addr);
        while (ri.hasNext()) {
            Reference rf = ri.next();
            Function f = fm.getFunctionContaining(rf.getFromAddress());
            if (f == null || f.equals(self)) continue;
            if (found == null) found = f;
            else if (!found.equals(f)) return null;
        }
        return found;
    }

    private Function nearestRealPreceding(long addr) {
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
