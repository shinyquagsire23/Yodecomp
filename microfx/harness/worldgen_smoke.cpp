// M0 smoke harness. Today: proves the microfx core + the ported game TUs compile, link, and
// basic core-class behavior holds. Destination (docs/phase-h4-sdl.md M0): load YODESK.DTA, run
// worldgen with a fixed seed, emit the YDBG log for diffing against a same-seed wine/Win32 run.
#include <afxwin.h>

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

int main()
{
    TestCString();
    TestArrays();
    TestCFile();
    if (g_nFail) { fprintf(stderr, "worldgen_smoke: %d FAILURES\n", g_nFail); return 1; }
    printf("worldgen_smoke: microfx core OK\n");
    return 0;
}
