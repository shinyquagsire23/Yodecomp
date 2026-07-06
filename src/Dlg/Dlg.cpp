// Dlg TU (0x418dd0–0x419000): CTextDialog.
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS
#include "Dlg.h"

// FUNCTION: YODA 0x00418dd0
// FUNCTION: YODA 0x00418ed0  (??_GCTextDialog scalar-deleting dtor — implicit, member-dtors inlined)
CTextDialog::CTextDialog(CWnd *pParent)
    : CDialog(0xbf, pParent)
{
    m_strField0 = "";
    m_strField1 = "";
    m_strField2 = "";
    m_strField3 = "";
}

// FUNCTION: YODA 0x00418f90
void CTextDialog::DoDataExchange(CDataExchange *pDX)
{
    DDX_Text(pDX, 0x7f, m_strField0);
    DDX_Text(pDX, 0x78, m_strField1);
    DDX_Text(pDX, 0x79, m_strField2);
    DDX_Text(pDX, 0x75, m_strField3);
}

// FUNCTION: YODA 0x00418fd0  (GetMessageMap)
BEGIN_MESSAGE_MAP(CTextDialog, CDialog)
END_MESSAGE_MAP()

// FUNCTION: YODA 0x00418fe0
BOOL CTextDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    CenterWindow();
    return TRUE;
}
