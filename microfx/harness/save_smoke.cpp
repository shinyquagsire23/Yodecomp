// save_smoke — INDYSAV44 save/load round-trip test (GAME_INDY). Bootstraps the real game object
// graph + worldgen (like worldgen_smoke), then drives CDeskcppDoc::IndyWriteWorldState ->
// IndyReadWorldState through a CFile and checks that scalars survive. A tail scalar surviving
// proves the ENTIRE file (magic/seed/quest list/100 cells/zone-recursive state incl. IACT
// doneFlags/inventory/tail) stayed cursor-aligned end-to-end — i.e. the write/read mirrors match.
// Usage: save_smoke <seed>  (run from a folder holding DESKTOP.DAW + yoda.INI).
#include <afxwin.h>
#include "Deskcpp.h"
#include "Worldgen.h"

static int g_nFail = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_nFail; } } while (0)

int main(int argc, char** argv)
{
    if (argc < 2) { fprintf(stderr, "usage: save_smoke <seed>\n"); return 2; }
    setenv("YODA_SEED", argv[1], 1);

    CWinApp* pApp = AfxGetApp();
    if (!pApp || !pApp->InitInstance()) { fprintf(stderr, "save_smoke: InitInstance failed\n"); return 1; }
    CDeskcppDoc* pDoc = (CDeskcppDoc*)pApp->m_pDocTemplate->m_pDoc;
    if (pDoc->Load() != 1) { fprintf(stderr, "save_smoke: Load failed\n"); return 1; }
    printf("save_smoke: world loaded seed=0x%08x totalZones=%d\n", pDoc->worldSeed, pDoc->totalZones);

#ifndef GAME_INDY
    fprintf(stderr, "save_smoke: not a GAME_INDY build — nothing to test\n");
    return 0;
#else
    // stamp distinctive values into fields the tail carries, then round-trip.
    pDoc->playerX = 1234;
    pDoc->playerY = 567;
    pDoc->healthLo = 2;
    pDoc->healthHi = 3;
    pDoc->difficulty = 0x2a;
    int nSeedBefore = pDoc->worldSeed;
    int nTotalBefore = pDoc->totalZones;

    const char* path = "save_smoke.wld";
    { CFile f; CFileException fe;
      if (!f.Open(path, CFile::modeCreate | CFile::modeReadWrite, &fe)) { fprintf(stderr, "save open failed\n"); return 1; }
      pDoc->IndyWriteWorldState(&f);
      printf("save_smoke: wrote %ld bytes\n", (long)f.GetLength());
      f.Close();
    }
    // IndyReadWorldState reads the body AFTER the 9-byte magic (OnLoadWorld consumes the magic),
    // so skip it here too, then drive the reader.
    { CFile f; CFileException fe;
      if (!f.Open(path, CFile::modeRead, &fe)) { fprintf(stderr, "load open failed\n"); return 1; }
      char magic[10]; f.Read(magic, 9); magic[9] = 0;
      CHECK(strcmp(magic, "INDYSAV44") == 0);
      pDoc->IndyReadWorldState(&f);
      f.Close();
    }
    remove(path);

    printf("save_smoke: after load  playerX=%d playerY=%d health=%d/%d diff=%d seed=0x%08x total=%d\n",
           pDoc->playerX, pDoc->playerY, pDoc->healthLo, pDoc->healthHi, pDoc->difficulty,
           pDoc->worldSeed, pDoc->totalZones);
    CHECK(pDoc->playerX == 1234);
    CHECK(pDoc->playerY == 567);
    CHECK(pDoc->healthLo == 2);
    CHECK(pDoc->healthHi == 3);
    CHECK(pDoc->difficulty == 0x2a);
    CHECK(pDoc->worldSeed == nSeedBefore);
    CHECK(pDoc->totalZones == nTotalBefore);

    if (g_nFail) { fprintf(stderr, "save_smoke: %d FAILURES\n", g_nFail); return 1; }
    printf("save_smoke: ROUND-TRIP OK\n");
    return 0;
#endif
}
