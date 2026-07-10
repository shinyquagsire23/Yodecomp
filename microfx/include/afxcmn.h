// microfx — <afxcmn.h> drop-in (YODA_PORTABLE). The game's only common control is the worldgen
// load-progress CProgressCtrl (Worldgen.cpp). (In the anchor build this header is also a codegen
// dial input — PLAN_COMPLETED.md v37 — but that is a byte-match concern, not a portable one.)
#ifndef MICROFX_AFXCMN_H
#define MICROFX_AFXCMN_H
#include <afxwin.h>

class CProgressCtrl : public CWnd
{
public:
    CProgressCtrl() : m_nLow(0), m_nHigh(100), m_nPos(0), m_nStep(10) {}
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    void SetRange(short nLower, short nUpper) { m_nLow = nLower; m_nHigh = nUpper; }
    void SetStep(int nStep) { m_nStep = nStep; }
    int  StepIt();
    int  SetPos(int nPos);

    int m_nLow, m_nHigh, m_nPos, m_nStep;
};

#endif // MICROFX_AFXCMN_H
