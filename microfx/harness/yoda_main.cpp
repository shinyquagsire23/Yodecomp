// The native game entry point (H4 M2) — the moral equivalent of MFC's AfxWinMain: the global
// theApp (Deskcpp.cpp) constructed itself before main; drive InitInstance (single-instance
// check, data path from GetModuleFileName, doc template, OnFileNew = full SDI bootstrap) and
// then Run (the SDL event pump, mfxpump.cpp). Data files: <exedir>/<GAME>.DTA and
// <exebase>.INI, same as the harnesses.
//
// NOT part of the microfx library (it defines main; the harnesses have their own).

#include <afxwin.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    CWinApp *pApp = AfxGetApp();
    if (!pApp) {
        fprintf(stderr, "yoda: no CWinApp constructed (theApp missing from the link?)\n");
        return 1;
    }

    // re-join the command line (the game parses "[OPEN ]<path>.WLD" for replay mode)
    static char szCmdLine[1024];
    szCmdLine[0] = 0;
    for (int i = 1; i < argc; i++) {
        if (i > 1) strncat(szCmdLine, " ", sizeof(szCmdLine) - strlen(szCmdLine) - 1);
        strncat(szCmdLine, argv[i], sizeof(szCmdLine) - strlen(szCmdLine) - 1);
    }
    pApp->m_lpCmdLine = szCmdLine;

    if (!pApp->InitInstance()) {
        pApp->ExitInstance();
        return 1;
    }
    int nRet = pApp->Run();
    pApp->ExitInstance();
    return nRet;
}
