// H4 M5 tail unit test (docs/phase-h4-sdl.md): CFileDialog's *.wld listing/resolve logic.
// The modal loop itself can't be driven headlessly (GetMessageA bails instantly with no SDL
// window — M4 lesson 12), so this exercises the pure helpers factored out of
// app/mfxdlg.cpp's CFileDialog::DoModal directly: directory scan, row building (Save's
// leading "(new)" row + overwrite labeling), and clicked-id -> path resolution (incl. the
// out-of-range fallback that guards against a stray accelerator WM_COMMAND sharing
// BN_CLICKED's HIWORD==0, see the comment at the interception site).
#include <afxwin.h>
#include <stdio.h>
#include <stdlib.h>
#include <filesystem>   // portable temp dir (was POSIX mkdtemp + /tmp)
#include <string>

int MfxFileDialogScan(const char *pszDir, const char *pszExt, CString aNames[], int nMax);
int MfxFileDialogBuildRows(int bOpenFileDialog, const CString &strDefault,
                           const CString aFound[], int nFound,
                           CString aRow[], UINT aRowId[], int nMaxRows, UINT idNew, UINT idRow0);
CString MfxFileDialogResolve(UINT nChosenId, UINT idNew, UINT idRow0, const CString &strDefault,
                             const CString aFound[], int nFound, const char *pszDir);

static int g_nFail = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_nFail; } } while (0)

enum { ID_NEW = 90, ID_ROW0 = 100 };

int main()
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::temp_directory_path(ec) / "dlg_smoke_test";
    std::filesystem::remove_all(dir, ec);                 // clean any prior run
    CHECK(std::filesystem::create_directories(dir, ec));
    const std::string strDir = dir.string();
    const char *szDir = strDir.c_str();

    // an empty save dir: Load finds nothing, Save offers only the "(new)" row
    {
        CString aFound[8];
        int nFound = MfxFileDialogScan(szDir, "wld", aFound, 8);
        CHECK(nFound == 0);

        CString strDefault = "savegame.wld";
        CString aRow[9]; UINT aRowId[9];
        int nRows = MfxFileDialogBuildRows(0 /*Save*/, strDefault, aFound, nFound, aRow, aRowId, 9, ID_NEW, ID_ROW0);
        CHECK(nRows == 1);
        CHECK(aRowId[0] == ID_NEW);
        CHECK(aRow[0] == "savegame.wld  (new)");

        nRows = MfxFileDialogBuildRows(1 /*Load*/, strDefault, aFound, nFound, aRow, aRowId, 9, ID_NEW, ID_ROW0);
        CHECK(nRows == 0);

        CString strPath = MfxFileDialogResolve(ID_NEW, ID_NEW, ID_ROW0, strDefault, aFound, nFound, szDir);
        CString strExpect = szDir; strExpect += "/savegame.wld";
        CHECK(strPath == strExpect);
    }

    // populate: savegame.wld (the default name) + two other slots + a non-matching file
    {
        char szPath[512];
        const char *aTouch[] = { "savegame.wld", "slot1.wld", "slot2.wld", "notes.txt" };
        for (int i = 0; i < 4; i++) {
            snprintf(szPath, sizeof szPath, "%s/%s", szDir, aTouch[i]);
            FILE *f = fopen(szPath, "w"); CHECK(f != 0); if (f) fclose(f);
        }

        CString aFound[8];
        int nFound = MfxFileDialogScan(szDir, "wld", aFound, 8);
        CHECK(nFound == 3);   // notes.txt excluded by extension

        CString strDefault = "savegame.wld";
        CString aRow[9]; UINT aRowId[9];
        int nRows = MfxFileDialogBuildRows(0 /*Save*/, strDefault, aFound, nFound, aRow, aRowId, 9, ID_NEW, ID_ROW0);
        CHECK(nRows == 3);                 // default already exists -> no separate "(new)" row
        int nOverwriteRow = -1, nSlot1Row = -1;
        for (int i = 0; i < nRows; i++) {
            if (aRow[i] == "savegame.wld  (overwrite)") nOverwriteRow = i;
            if (aRow[i] == "slot1.wld") nSlot1Row = i;
        }
        CHECK(nOverwriteRow >= 0);
        CHECK(nSlot1Row >= 0);

        // resolve the overwrite row -> savegame.wld's real path
        CString strPath = MfxFileDialogResolve(aRowId[nOverwriteRow], ID_NEW, ID_ROW0, strDefault, aFound, nFound, szDir);
        CString strExpect = szDir; strExpect += "/savegame.wld";
        CHECK(strPath == strExpect);

        // resolve slot1's row -> slot1.wld's real path
        strPath = MfxFileDialogResolve(aRowId[nSlot1Row], ID_NEW, ID_ROW0, strDefault, aFound, nFound, szDir);
        strExpect = szDir; strExpect += "/slot1.wld";
        CHECK(strPath == strExpect);

        // Load mode: all 3 rows, no "(new)" row, no "(overwrite)" suffix
        nRows = MfxFileDialogBuildRows(1 /*Load*/, strDefault, aFound, nFound, aRow, aRowId, 9, ID_NEW, ID_ROW0);
        CHECK(nRows == 3);
        for (int i = 0; i < nRows; i++) CHECK(aRow[i].Find("(overwrite)") < 0 && aRow[i].Find("(new)") < 0);

        // regression guard: a stray accelerator WM_COMMAND (e.g. 0xe140 About) shares
        // BN_CLICKED's HIWORD==0 — resolving it must NOT index aFound[] out of bounds, and
        // must fall back to the default name rather than garbage/crash.
        strPath = MfxFileDialogResolve(0xe140, ID_NEW, ID_ROW0, strDefault, aFound, nFound, szDir);
        strExpect = szDir; strExpect += "/savegame.wld";
        CHECK(strPath == strExpect);
    }

    if (g_nFail) { printf("dlg_smoke: %d check(s) FAILED\n", g_nFail); return 1; }
    printf("dlg_smoke: all checks passed\n");
    return 0;
}
