// Minimal static-MFC app to validate the NAFXCW.LIB compile+link path.
#include <afxwin.h>
#include <afxcoll.h>   // CObArray / CDWordArray — what Zone uses

class CTestApp : public CWinApp {
public:
    virtual BOOL InitInstance();
};

BOOL CTestApp::InitInstance()
{
    CDWordArray a;      // exercise the MFC collection ctor/dtor (NAFXCW)
    a.SetSize(4, -1);
    a[0] = 0x1234;
    return FALSE;       // don't enter the message loop; exit
}

CTestApp theApp;
