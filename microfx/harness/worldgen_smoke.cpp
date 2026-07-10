// M0 smoke harness. No args: microfx core-class self-tests only. With a seed argument
// (`worldgen_smoke 0x2a [data.dta]`): bootstrap the real game object graph (theApp's
// InitInstance → doc template → CWinApp::OnFileNew → CDeskcppDoc), run the .dta load +
// worldgen at that pinned seed, and (built with -DYODA_DEBUG=ON) emit the WORLD/CELL digest
// to yoda_debug.log for diffing against a same-seed wine/Win32 run (docs/phase-h4-sdl.md M0).
#include <afxwin.h>
#include "Deskcpp.h"     // CDeskcppApp (m_str = data-file path)
#include "Worldgen.h"    // CDeskcppDoc facade: Load(), worldSeed, totalZones, nZonesLoaded

static int g_nFail = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_nFail; } } while (0)

static void TestCString()
{
    CString a("hello");
    CHECK(a.GetLength() == 5);
    CHECK(sizeof(CString) == sizeof(char*));      // MFC layout invariant (embedded in structs)
    a += " world";
    CHECK(a == "hello world");
    CHECK(a.Left(5) == "hello");
    CHECK(a.Right(5) == "world");
    CHECK(a.Mid(6, 5) == "world");
    CHECK(a.Find('w') == 6);
    CHECK(a.Find("world") == 6);
    a.MakeUpper();
    CHECK(a == "HELLO WORLD");
    CHECK(a.CompareNoCase("hello world") == 0);
    CString b;
    b.Format("%d-%s", 42, "x");
    CHECK(b == "42-x");
    CString c(a);
    c = c;                                        // self-assign
    CHECK(c == "HELLO WORLD");
}

static void TestArrays()
{
    CWordArray wa;
    wa.SetAtGrow(4, 7);
    CHECK(wa.GetSize() == 5 && wa.GetAt(4) == 7 && wa.GetAt(0) == 0);
    wa.SetSize(2);
    CHECK(wa.GetSize() == 2);
    wa.Add(9);
    CHECK(wa.GetAt(2) == 9);
    wa.RemoveAt(0);
    CHECK(wa.GetSize() == 2 && wa.GetAt(1) == 9);

    CObArray oa;
    CObject* p = (CObject*)0x1234;                // pointer stored, never dereferenced
    oa.Add(p);
    CHECK(oa.GetSize() == 1 && oa[0] == p);
    oa.InsertAt(0, 0);
    CHECK(oa.GetSize() == 2 && oa[1] == p);
}

static void TestCFile()
{
    const char* path = "mfx_smoke.tmp";
    TRY
    {
        CFile f;
        CFileException fe;
        CHECK(f.Open(path, CFile::modeCreate | CFile::modeReadWrite, &fe));
        int v = 0x12345678;
        f.Write(&v, 4);
        CHECK(f.GetLength() == 4);
        f.SeekToBegin();
        int r = 0;
        CHECK(f.Read(&r, 4) == 4 && r == v);
        f.Close();
    }
    CATCH_ALL(e)
    {
        CHECK(!"CFile threw");
    }
    END_CATCH_ALL
    remove(path);

    // missing file must fill the exception, not crash
    CFile g;
    CFileException fe2;
    CHECK(!g.Open("definitely_missing.bin", CFile::modeRead, &fe2));
    CHECK(fe2.m_cause == CFileException::fileNotFound);
}

// Bootstrap the game exactly the way WinMain would: virtual InitInstance on the global theApp
// (single-instance check, data path from GetModuleFileName, doc template, OnFileNew), then run
// the .dta load + worldgen with the seed pinned via YODA_SEED (read by Randomize).
static int RunWorldgen(const char* pszSeed, const char* pszDataOverride)
{
    if (strcmp(pszSeed, "-") == 0)
        unsetenv("YODA_SEED");          // "-" = unpinned: Randomize() rolls real seeds
    else
        setenv("YODA_SEED", pszSeed, 1);
    CWinApp* pApp = AfxGetApp();
    if (!pApp) { fprintf(stderr, "worldgen_smoke: no theApp\n"); return 1; }
    if (!pApp->InitInstance()) { fprintf(stderr, "worldgen_smoke: InitInstance failed\n"); return 1; }
    if (pszDataOverride)
        ((CDeskcppApp*)pApp)->m_str = pszDataOverride;
    printf("worldgen_smoke: data=%s\n", (const char*)((CDeskcppApp*)pApp)->m_str);
    if (!pApp->m_pDocTemplate || !pApp->m_pDocTemplate->m_pDoc)
        { fprintf(stderr, "worldgen_smoke: OnFileNew produced no document\n"); return 1; }
    CDeskcppDoc* pDoc = (CDeskcppDoc*)pApp->m_pDocTemplate->m_pDoc;
    int nOk = pDoc->Load();
    printf("worldgen_smoke: Load=%d seed=0x%08x zonesLoaded=%d totalZones=%d\n",
           nOk, pDoc->worldSeed, pDoc->nZonesLoaded, pDoc->totalZones);
    return nOk == 1 ? 0 : 1;
}

int main(int argc, char** argv)
{
    TestCString();
    TestArrays();
    TestCFile();
    if (g_nFail) { fprintf(stderr, "worldgen_smoke: %d FAILURES\n", g_nFail); return 1; }
    printf("worldgen_smoke: microfx core OK\n");
    if (argc > 1)
        return RunWorldgen(argv[1], argc > 2 ? argv[2] : 0);
    return 0;
}
