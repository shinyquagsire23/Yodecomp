// Scan for undefined code between defined functions and (optionally) create
// functions from it. Functions in YodaDemo.exe are 16-byte aligned and the
// inter-function slack is padded with 0xCC (some 0x00). A "gap" here is any
// address in [SCAN_START, SCAN_END) that is not covered by a function body.
//
// WHY THIS IS POLICY-DRIVEN (read before running AGGRESSIVE):
// An earlier prepass (see CLAUDE.md) established that Ghidra's function coverage
// of YodaDemo's app region is already complete -- every direct CALL target is a
// function. The bytes left in the gaps are NOT missed functions; they are:
//   * MSVC 4.2 C++ EH continuation/cleanup funclets (10-byte `mov eax,imm;jmp`
//     and tiny `unaff_EBP`-frame destructor stubs) -- referenced only from the
//     .rdata exception tables (DATA refs). These BELONG TO A PARENT function.
//   * switch/case blocks reached by an internal JMP (jump-table dispatch).
//   * raw jump-table bytes / dead code with no incoming reference at all.
// Blindly turning all of these into standalone functions produces the exact
// "overlapping garbage bodies" that was tried at 0x403501-0x40379f and reverted.
//
// The classifier below sorts each candidate by its STRONGEST incoming reference
// and only acts per POLICY:
//   SAFE       (default): create only at CALL targets = genuine missed functions.
//   FUNCLETS  : also create at DATA-ref targets (EH/vtable funclets). Names them
//               `gap_ehlike_<addr>` -- use only if you WANT the funclets promoted
//               to functions (e.g. so vtable/EH-table refs resolve to a symbol).
//   AGGRESSIVE: also create at JMP targets and unreferenced runs. Very likely
//               garbage; intended for exploratory disassembly only.
//
// DRY_RUN=true reports what each policy WOULD do without mutating the program.
// Every creation is reversible via Ghidra's "Delete Function" / delete_function.
//
// Guards applied before any create (borrowed from the vtable-recovery pass):
//   - target must be a valid instruction start (disassembles cleanly), and
//   - target must not already be inside an existing function.
// These skip jump-table CASE labels and mid-instruction bytes -> no bad splits.
//
//@category Yodecomp
//@author Yodecomp

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.address.AddressRangeIterator;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.ReferenceManager;

public class CreateGapFunctions extends GhidraScript {

    // ---- configuration -----------------------------------------------------
    // App-region text span for YodaDemo.exe (see CLAUDE.md .text layout table).
    private static final long SCAN_START = 0x401000L;
    private static final long SCAN_END   = 0x44b000L;

    // true  = report only, do not modify the program (recommended first pass).
    // false = actually create functions per POLICY.
    private static final boolean DRY_RUN = true;

    // SAFE | FUNCLETS | AGGRESSIVE  (see header).
    private static final String POLICY = "SAFE";

    // Bytes treated as inter-function padding to skip when finding the first
    // real byte of a gap run.
    private static boolean isPad(int b) { return b == 0xCC || b == 0x00; }

    // ------------------------------------------------------------------------
    @Override
    public void run() throws Exception {
        Listing listing = currentProgram.getListing();
        Memory mem = currentProgram.getMemory();
        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceManager rm = currentProgram.getReferenceManager();

        boolean doFunclets   = POLICY.equals("FUNCLETS") || POLICY.equals("AGGRESSIVE");
        boolean doAggressive = POLICY.equals("AGGRESSIVE");

        // 1) union of all function bodies in range
        AddressSet funcSet = new AddressSet();
        FunctionIterator fit = fm.getFunctions(true);
        while (fit.hasNext()) {
            Function f = fit.next();
            long a = f.getEntryPoint().getOffset();
            if (a < SCAN_START || a >= SCAN_END) continue;
            funcSet.add(f.getBody());
        }

        // 2) gaps = [start,end) minus function bodies
        AddressSet all = new AddressSet(toAddr(SCAN_START), toAddr(SCAN_END - 1));
        AddressSet gaps = all.subtract(funcSet);

        int nCall = 0, nData = 0, nJmp = 0, nNone = 0;
        int created = 0, skippedInFunc = 0, skippedBadInsn = 0, skippedPolicy = 0;
        StringBuilder log = new StringBuilder();

        AddressRangeIterator rit = gaps.getAddressRanges();
        while (rit.hasNext()) {
            AddressRange r = rit.next();
            long lo = r.getMinAddress().getOffset();
            long hi = r.getMaxAddress().getOffset();

            // skip leading padding to the first real byte
            long a = lo;
            while (a <= hi && isPad(mem.getByte(toAddr(a)) & 0xff)) a++;
            if (a > hi) continue; // pure-padding gap

            Address ca = toAddr(a);

            // classify by strongest incoming reference
            boolean hasCall = false, hasJmp = false, hasData = false;
            ReferenceIterator ri = rm.getReferencesTo(ca);
            while (ri.hasNext()) {
                RefType rt = ri.next().getReferenceType();
                if (rt.isCall()) hasCall = true;
                else if (rt.isJump()) hasJmp = true;
                else hasData = true;
            }
            String cls;
            boolean want;
            if (hasCall)      { cls = "CALL"; nCall++; want = true; }
            else if (hasData) { cls = "DATA"; nData++; want = doFunclets; }
            else if (hasJmp)  { cls = "JMP";  nJmp++;  want = doAggressive; }
            else              { cls = "NONE"; nNone++; want = doAggressive; }

            if (!want) { skippedPolicy++; continue; }

            // guard: must not already be inside a function (defensive; gap set
            // already excludes bodies, but a prior create in this run may cover it)
            if (fm.getFunctionContaining(ca) != null) { skippedInFunc++; continue; }

            // guard: must be a valid instruction start
            Instruction ins = listing.getInstructionAt(ca);
            if (ins == null) {
                if (!DRY_RUN) disassemble(ca);
                ins = listing.getInstructionAt(ca);
            }
            if (ins == null) { skippedBadInsn++; continue; }

            String name = cls.equals("CALL") ? null // let Ghidra name FUN_<addr>
                        : cls.equals("DATA") ? "gap_ehlike_" + Long.toHexString(a)
                        : "gap_code_" + Long.toHexString(a);

            if (DRY_RUN) {
                log.append(String.format("WOULD-CREATE %08x [%s] %s\n", a, cls,
                        name == null ? "FUN_" + Long.toHexString(a) : name));
            } else {
                Function nf = createFunction(ca, name);
                if (nf != null) {
                    created++;
                    log.append(String.format("CREATED %08x [%s] %s\n", a, cls, nf.getName()));
                } else {
                    skippedBadInsn++;
                }
            }
        }

        println("=== CreateGapFunctions (" + (DRY_RUN ? "DRY-RUN" : "LIVE") + ", policy=" + POLICY + ") ===");
        println("range = [0x" + Long.toHexString(SCAN_START) + ", 0x" + Long.toHexString(SCAN_END) + ")");
        println("gap candidates by strongest ref:  CALL=" + nCall + "  DATA=" + nData
                + "  JMP=" + nJmp + "  NONE=" + nNone);
        println("created=" + created + "  skipped(policy)=" + skippedPolicy
                + "  skipped(already-in-func)=" + skippedInFunc
                + "  skipped(not-instruction)=" + skippedBadInsn);
        println(log.toString());
    }
}
