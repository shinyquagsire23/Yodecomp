// Dlg TU (0x418dd0–0x419000): CTextDialog, a CDialog-derived text-entry dialog (resource 0xbf)
// with four edit fields. Constructed from GameView::OnKeyDown.
#ifndef DLG_H
#define DLG_H
#include <afxwin.h>

class CTextDialog : public CDialog
{
public:                              // vftable 0x0044bd30; CDialog base ends at 0x5c
    CString m_strField0;             // +0x5c  DDX id 0x7f
    CString m_strField1;             // +0x60  DDX id 0x78
    CString m_strField2;             // +0x64  DDX id 0x79
    CString m_strField3;             // +0x68  DDX id 0x75
                                     // sizeof 0x6c (proven by GameView::OnKeyDown's stack frame:
                                     // dlg at [EBP-0x94], SUB ESP,0x88; members end at +0x6c).
                                     // (was mistakenly padded to 0xc8 — that is the unrelated
                                     //  game TextDialog@0x416b90's size.)

    CTextDialog(CWnd *pParent);                          // 0x00418dd0
    virtual void DoDataExchange(CDataExchange *pDX);     // 0x00418f90
    virtual BOOL OnInitDialog();                         // 0x00418fe0
protected:
    DECLARE_MESSAGE_MAP()                                // 0x00418fd0
};

#endif
