// GameView TU (0x4084f0-0x418700): the CDeskcppView class (our GameView) plus its embedded
// helper classes (InvScrollBar, TextDialog, the balloon/option dialogs). ONE source file /
// ONE .obj — no exception-COMDAT clusters mark an internal boundary and the functions flow
// contiguously (InvScrollBar's ctor/dtor is interleaved between GameView's DYNCREATE statics
// and GameView::GameView, which MSVC would never do across .objs). Phase E step 4:
// transcribe in .text address order. See docs/game-logic.md for the runtime frame loop.
//
// Cross-TU calls (World::*, GameData helpers, Canvas) reach through Worldgen.h's shared facade;
// the GameView method-decl set (the dial) lives in GameView.h, included via Worldgen.h.
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "Worldgen.h"
#include "TextDialog.h"

// IactProbeMove's a5 arg carries &pWorld->tiles through an `int` param the callee never reads
// (faithful to the original 32-bit call). On the 64-bit portable build that cast is ill-formed,
// so the sites cast through PTRINT: anchor preprocesses to the ORIGINAL `(int)` tokens.
#ifdef YODA_PORTABLE
#define PTRINT intptr_t
#else
#define PTRINT int
#endif

// --- WAVMIX32 imports (all __stdcall) -----------------------------------------
extern "C" {
    UINT WINAPI WaveMixPump(void);
    int  WINAPI WaveMixInit(void);
    int  WINAPI WaveMixActivate(int hMixSession, BOOL fActivate);
    int  WINAPI WaveMixOpenWave(int hMixSession, char *szWaveFilename, int hInst, DWORD dwFlags);
    int  WINAPI WaveMixOpenChannel(int hMixSession, int iChannel, DWORD dwFlags);
    int  WINAPI WaveMixFreeWave(int hMixSession, int lpMixWave);
    int  WINAPI WaveMixCloseChannel(int hMixSession, int iChannel, DWORD dwFlags);
    int  WINAPI WaveMixFlushChannel(int hMixSession, int iChannel, DWORD dwFlags);
    int  WINAPI WaveMixCloseSession(int hMixSession);
    int  WINAPI WaveMixPlay(void *lpMixPlayParams);
}
// WAVMIX32 MIXPLAYPARAMS (packed — a WORD then 4-byte fields at unaligned offsets).
#pragma pack(push, 1)
struct MIXPLAYPARAMS
{
    WORD wSize;         // +0x00
    int  hMixSession;   // +0x02
    int  iChannel;      // +0x06
    int  lpMixWave;     // +0x0a
    int  hWndNotify;    // +0x0e
    int  dwFlags;       // +0x12
    WORD wReserved;     // +0x16   (struct size 0x18)
};
#pragma pack(pop)

// --- module globals -----------------------------------------------------------
char  *g_pszFontName = "MS Sans Serif";  // 0x00456130  (read by OnTimer + DrawText)
char  *g_pszDialogFont = "MS Sans Serif"; // 0x004561cc (read by TextDialog::Run + ::Layout)
int    g_bStopMusicThread;   // 0x00456134  set nonzero to end the pump loop
HANDLE g_hWaveMixEvent;      // 0x00459454  pump-tick event
int    g_dat459450;          // 0x00459450  cleared by the GameView ctor (music/sound related)
int    g_waveHandles[64];    // 0x00459458..0x00459558  loaded-wave handle table (SoundInit fills)

#ifdef GAME_INDY
// =============================================================================
// Indy MIDI music (GAME_INDY) — RE'd from DESKADV.EXE (see IndySoundId in DeskcppView.h).
// Music is MCI sequencer command strings: "open sequencer!<file> alias <NAME>" /
// "play <NAME> from 1" / "stop <NAME>" / "close <NAME>", with a per-id opened flag
// (the 16-bit DGROUP word table @0x53c indexed by sound id). Independent of the
// WaveMix SFX session. The 16-bit engine also has a themw3.mid fallback branch for
// VERS-0x1e-era data (doc+0x56 == 0x1e) — DESKTOP.DAW is VERS 0x200, not reproduced.
// =============================================================================
#include <mmsystem.h>
#undef PlaySound   // mmsystem.h's PlaySound->PlaySoundA macro would clobber our method name
#include "DebugLog.h"   // YDBG compiles to nothing unless -D YODA_DEBUG

static int  g_abIndyMidiOpen[64];        // per-sound-id "sequencer open" flag
static char g_aszIndyMidiAlias[64][16];  // per-sound-id MCI alias ("THEME", "EERIE", ...)

// "open sequencer!<file> alias <NAME>"; alias = basename sans extension (names arrive
// already basenamed + uppercased from ParseSnds/_splitpath).
static void Indy_MidiOpen(int nSoundId, const char *pszFile)
{
    char szCmd[100];
    char *pszAlias = g_aszIndyMidiAlias[nSoundId];
    strcpy(pszAlias, pszFile);
    char *pDot = strchr(pszAlias, '.');
    if (pDot != NULL)
        *pDot = 0;
    wsprintf(szCmd, "open sequencer!%s alias %s", pszFile, pszAlias);
    DWORD nErr = mciSendString(szCmd, NULL, 0, NULL);
    if (nErr == 0)
        g_abIndyMidiOpen[nSoundId] = 1;
    YDBG(("MidiOpen id=0x%x cmd=\"%s\" err=%lu\n", nSoundId, szCmd, nErr));
}

// Open every .MID named in SNDS plus the hardcoded eerie.mid (DESKADV FUN_1018_4c54 tail;
// eerie is sound id SND_INDY_EERIE, outside the SNDS list). Idempotent per id.
static void Indy_MidiOpenAll(CDeskcppDoc *pWorld)
{
    for (int i = 0; i < 64; i++)
    {
        if (g_abIndyMidiOpen[i] == 0 && pWorld->soundNames[i].GetLength() > 0
            && strstr(pWorld->soundNames[i], ".MID") != NULL)
            Indy_MidiOpen(i, pWorld->soundNames[i]);
    }
    if (g_abIndyMidiOpen[SND_INDY_EERIE] == 0)
        Indy_MidiOpen(SND_INDY_EERIE, "eerie.mid");
}

// DESKADV FUN_1010_e43c music arm / FUN_1018_6dd0: play if opened (caller gates nMusicEnabled).
static void Indy_MidiPlay(int nSoundId)
{
    char szCmd[100];
    if (nSoundId < 0 || nSoundId >= 64 || g_abIndyMidiOpen[nSoundId] == 0)
        return;
    wsprintf(szCmd, "play %s from 1", g_aszIndyMidiAlias[nSoundId]);
    DWORD nErr = mciSendString(szCmd, NULL, 0, NULL);
    YDBG(("MidiPlay id=0x%x cmd=\"%s\" err=%lu\n", nSoundId, szCmd, nErr));
}

// DESKADV FUN_1018_6e34 — stop every opened sequencer (ToggleMusic off, new game).
void Indy_MidiStopAll()
{
    char szCmd[100];
    for (int i = 0; i < 64; i++)
    {
        if (g_abIndyMidiOpen[i] == 0)
            continue;
        wsprintf(szCmd, "stop %s", g_aszIndyMidiAlias[i]);
        mciSendString(szCmd, NULL, 0, NULL);
    }
}

// DESKADV FUN_1010_dff0 head — close every opened sequencer (view teardown).
static void Indy_MidiCloseAll()
{
    char szCmd[100];
    for (int i = 0; i < 64; i++)
    {
        if (g_abIndyMidiOpen[i] == 0)
            continue;
        wsprintf(szCmd, "close %s", g_aszIndyMidiAlias[i]);
        mciSendString(szCmd, NULL, 0, NULL);
        g_abIndyMidiOpen[i] = 0;
    }
}

// Shared engine code hardcodes YODA sound ids, but the two games' SNDS tables differ
// (Yoda 5=eep/6=nogo vs Indy 5=ROAR/6=DOOR/8=NOGO...). Translate at the PlaySound
// boundary; data-driven ids (IACT args, weapon sounds) bypass via PlaySoundData.
// -1 = no Indy equivalent (silent). VERIFIED v73 vs DESKADV: all 28 IndyPlaySound call
// sites push only {0,1,3,4,8,0x0e-0x11,0x13}; whole -1 set + 0xb->7 CONFIRMED correct.
static int Indy_MapSoundId(int nYodaSoundId)
{
    switch (nYodaSoundId)
    {
    case 5:    return SND_INDY_EEP;       // eep.wav -> the hardcoded 15th wave
    case 6:    return 8;                  // nogo.wav -> NOGO.WAV
    case 0xb:  return 7;                  // banglrg.wav -> EXPLODE.WAV
    case 0x3a: return SND_INDY_FLOURISH;  // flourish.wav -> FLOURISH.MID (new game)
    case 0x3d: return SND_INDY_THEME;     // opening.wav -> THEME.MID (startup)
    case 0x3e: return SND_INDY_DEFEAT;    // tryagain.wav -> DEFEAT.MID
    case 0x3f: return SND_INDY_VICTORY;   // youwin.wav -> VICTORY.MID
    case 0x1f: case 0x20: case 0x21: case 0x22: case 0x23:
    case 0x2a: case 0x2b: case 0x31: case 0x34: case 0x37:
        return -1;                        // Yoda-only concept; Indy has no equivalent sound
    default:   return nYodaSoundId;       // 0-4 etc. line up 1:1
    }
}
#endif // GAME_INDY

// =============================================================================
// GameView-TU-private option dialogs (0x416810-0x4186e0). Declared here (not in the
// shared GameView.h) so the doc TU that includes that header does not see them and its
// codegen dial stays put. StatsDlg (demo stats, 4 CString DDX fields, template 0xe1) +
// three near-identical CDialog sliders (CDialog + int m_nValue@0x5c; OnInitDialog sizes
// a scrollbar control to the setting's range and seeds the thumb; OnHScroll steps it).
// Class names follow the Ghidra namespaces (by the OnCmd* handler that runs each).
// =============================================================================
class StatsDlg : public CDialog
{
public:
    int      unk5c;                  // +0x5c
    CDeskcppDoc   *pWorld;                 // +0x60  ctor arg (the doc)
    CString  m_str0;                 // +0x64  DDX 0x98 <- World.lastCount
    CString  m_str1;                 // +0x68  DDX 0x97 <- World.completionCount
    CString  m_str2;                 // +0x6c  DDX 0x95 <- World.highScore
    CString  m_str3;                 // +0x70  DDX 0x96 <- World.lastScore
    StatsDlg(CWnd *pParent, CDeskcppDoc *pDoc);                // 0x00416810
    virtual void DoDataExchange(CDataExchange *pDX);     // 0x004169e0
    virtual BOOL OnInitDialog();                         // 0x00416a40
    DECLARE_MESSAGE_MAP()
};

class DifficultyDlg : public CDialog
{
public:
    int m_nValue;                    // +0x5c  slider value
    DifficultyDlg(CWnd *pParent);                        // 0x00417e50 (template 0x6f)
    virtual void DoDataExchange(CDataExchange *pDX);     // 0x00417f30 EMPTY (vtable slot +0x88; bare RET 4)
    virtual BOOL OnInitDialog();                         // 0x00417f50 (ctrl 0x67, 1..100)
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar); // 0x00417fa0
    DECLARE_MESSAGE_MAP()
};

class GameSpeedDlg : public CDialog
{
public:
    int m_nValue;                    // +0x5c  slider value
    GameSpeedDlg(CWnd *pParent);                         // 0x00418130 (template 0xd7)
    virtual void DoDataExchange(CDataExchange *pDX);     // 0x00418210 EMPTY (bare RET 4)
    virtual BOOL OnInitDialog();                         // 0x00418230 (ctrl 0x8f, 1..0x5a)
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar); // 0x00418280
    DECLARE_MESSAGE_MAP()
};

class WorldSizeDlg : public CDialog
{
public:
    int m_nValue;                    // +0x5c  slider value
    WorldSizeDlg(CWnd *pParent);                         // 0x00418410 (template 0xda)
    virtual void DoDataExchange(CDataExchange *pDX);     // 0x004184f0 EMPTY (bare RET 4)
    virtual BOOL OnInitDialog();                         // 0x00418510 (ctrl 0x90, 1..3)
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar); // 0x00418560
    DECLARE_MESSAGE_MAP()
};

// =============================================================================
// DYNCREATE + message maps (macro-generated: CreateObject 0x4084f0,
// GetRuntimeClass 0x408560, GameView::GetMessageMap 0x408570,
// InvScrollBar::GetMessageMap 0x408580).
// =============================================================================
// FUNCTION: YODA 0x004084f0
// FUNCTION: YODA 0x00408560 (GetRuntimeClass)
IMPLEMENT_DYNCREATE(CDeskcppView, CView)

// FUNCTION: YODA 0x00408570
BEGIN_MESSAGE_MAP(CDeskcppView, CView)
    //{{AFX_MSG_MAP(GameView)
    // v49: entry ORDER reconciled to the original AFX_MSGMAP_ENTRY array (msgcheck-verified) —
    // #11 WM_VSCROLL (was the ON_WM_HSCROLL bug), the difficulty ON_UPDATE moved after worldsize,
    // stats moved to the tail after WM_CHAR, and the dialog Up/Down buttons in original order.
    ON_COMMAND(0x8001, OnCmdMinimize)                       // #0
    ON_WM_LBUTTONDOWN()                                     // #1
    ON_WM_LBUTTONUP()                                       // #2
    ON_WM_SETCURSOR()                                       // #3
    ON_WM_MOUSEMOVE()                                       // #4
    ON_WM_ERASEBKGND()                                      // #5
    ON_WM_RBUTTONDOWN()                                     // #6
    ON_WM_KEYDOWN()                                         // #7
    ON_WM_KEYUP()                                           // #8
    ON_WM_TIMER()                                           // #9
    ON_WM_DESTROY()                                         // #10
    ON_WM_VSCROLL()                                         // #11 (inventory scrollbar is VERTICAL)
    ON_COMMAND(ID_APP_EXIT, OnAppExit)                      // #12
    ON_COMMAND(0x8005, OnCmdDifficulty)                     // #13
    ON_COMMAND(0x8002, OnTogglePause)                       // #14
    ON_UPDATE_COMMAND_UI(0x8002, OnUpdatePauseUi)           // #15
    ON_COMMAND(0x800c, OnCmdGameSpeed)                      // #16
    ON_UPDATE_COMMAND_UI(0x800c, OnUpdateGameSpeedUi)       // #17
    ON_COMMAND(0x800d, OnCmdWorldSizeMaybe)                 // #18
    ON_UPDATE_COMMAND_UI(0x800d, OnUpdateWorldSizeUi)       // #19
    ON_UPDATE_COMMAND_UI(0x8005, OnUpdateDifficultyUi)      // #20 (difficulty update trails, not paired)
    ON_BN_CLICKED(0x1389, OnDialogCloseBtn)                 // #21
    ON_BN_CLICKED(0x138b, OnDialogUpBtnNop)                 // #22
    ON_BN_CLICKED(0x138a, OnDialogDownBtnNop)               // #23
    ON_WM_CTLCOLOR()                                        // #24
    ON_WM_CHAR()                                            // #25
    ON_COMMAND(0x800e, OnCmdStats)                          // #26
    ON_UPDATE_COMMAND_UI(0x800e, OnUpdateStatsUi)           // #27
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

// FUNCTION: YODA 0x00408580
BEGIN_MESSAGE_MAP(InvScrollBar, CScrollBar)
    //{{AFX_MSG_MAP(InvScrollBar)
    ON_WM_VSCROLL()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00408590
// Music pump worker thread (AfxBeginThread proc): keeps WAVMIX32 fed until the
// stop flag is set. __cdecl (static member).
UINT CDeskcppView::MusicThreadProcMaybe(void *pParam)
{
    while (g_bStopMusicThread == 0)
    {
        WaveMixPump();
        WaitForSingleObject(g_hWaveMixEvent, 100);
    }
    return 0;
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x004085c0
// InvScrollBar::InvScrollBar — build the inventory scroll bar as a child of the
// view, at World's inventory-scrollbar rect (@0x3294), control id 0x65.
InvScrollBar::InvScrollBar(CDeskcppView *pView, RECT *pRect)
{
    Create(0x50000001, *pRect, pView, 0x65);
    ::SetScrollRange(m_hWnd, SB_CTL, 0, 1, FALSE);
    ::SetScrollPos(m_hWnd, SB_CTL, 0, FALSE);
    ::ShowScrollBar(m_hWnd, SB_CTL, TRUE);
    scrollMax = 0;
    scrollPos = 0;
}
// InvScrollBar's explicit dtor (??1 0x4086b0 + thin ??_G 0x408690) is DEFINED AT THE END of
// this file (not here) so its added source lines don't shift the #line provenance of the
// functions below and rotate the GameView TU dial. See the definition + autopsy there.

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00408710
// GameView::GameView — DYNCREATE default ctor. The CString (strCheatBuffer), three
// CBitmapButton (btnDialog*) and CEdit (wndDialogText) members are constructed
// implicitly by the compiler-generated member-init prologue; the body zero/inits the
// plain scalar fields in source order (NOT sorted — matched literally).
// EFFECTIVE (align~94, ~89% identical / 265 vs 264 insns): field-init order is faithful
//   to the decompile. Two known-open residuals, both G1 fodder (NOT source misses):
//   (a) imm/reg STORE-SCHEDULING — MSVC reschedules the run of `= 0`(edi) / `= -1`(ebx)
//       stores and materializes `mov ebx,-1` at a different point than the original; the
//       identical open mechanism parked in the WorldDoc World ctor (DIFF~510). Source
//       order matches; the compiler batches by value/register anyway.
//   (b) member-ctor INLINE-vs-OUT-OF-LINE: one CBitmapButton's embedded CBitmap (@+0x22c)
//       is constructed inline in the original but the compiler chose differently here — a
//       TU-context inlining-threshold artifact (lesson #7/#8), expected to settle as the
//       rest of the TU lands.
CDeskcppView::CDeskcppView()
{
    unk178 = 0;
    bInvincibleCheat = 0;
    bOneShotStubMaybe = 0;
    bKeyboardMoveActive = 0;
    bIactZoneEntryMaybe = 0;
    bMouseCaptured = 0;
    bMidiProfileInitMaybe = 0;
    bPickupClickPendingMaybe = 0;
    bInvClickPending = 0;
    nTransitionStep = -1;
    nSavedFrameMode = 3;
    pWorld = 0;
    pInvScrollBar = 0;
    bInitialized = 0;
    nPaletteClock = 0;
    frameCounter = 0;
    bDebugFlagMaybe = 0;
    bShiftHeld = 0;
    nDragLastScreenY = -1;
    nDragLastScreenX = -1;
    nMovePending = 0;
    nDragSlot = -1;
    nMoveCommand = -1;
    unkB8_always1 = 1;
    bBusy = 1;
    soundSession = 0;
    g_dat459450 = 0;
    bMapTeleportEnabled = 0;
    bViewActive = 0;
    nGameSpeed = AfxGetApp()->GetProfileInt("OPTIONS", "GameSpeed", 0x8c);
    nDetonatorPhase = 0;
    bPauseOverlayDrawn = 0;
    bDialogCloseClicked = 0;
    unk2e0 = 0;
    bTextDialogShown = 0;
    bDialogClickDismissMaybe = 0;
    bBlockBumpUntilClick = 0;
    pMusicThread = 0;
    unk2d4 = 0;
    unk2d8 = 0;
    artooAnyhowHelpIdx = 0;
    bArtooBeepPending0Maybe = 0;
    bDropOnArtooMaybe = 0;
    bDraggedArtooBlockedMaybe = 0;
    bDropOutsideViewMaybe = 0;
    bLocatorKeyLatchMaybe = 0;
    unk2e8_always1 = 1;
    bShowEmptyDialogOnceMaybe = 1;
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00408c60
// GameView::~GameView — stop the music pump, tear down the WAVMIX32 session and all
// loaded waves, free the drag save-bit buffers + drag-tile canvas. The CString /
// CBitmapButton x3 / CEdit members are destroyed by the compiler-generated epilogue.
CDeskcppView::~CDeskcppView()
{
    g_bStopMusicThread = 1;
#ifdef GAME_INDY
    Indy_MidiCloseAll();   // DESKADV FUN_1010_dff0 closes the MCI sequencers first
#endif
    if (soundSession != 0)
    {
        WaveMixActivate(soundSession, FALSE);
        int *p = g_waveHandles;
        do
        {
            if (*p != 0)
                WaveMixFreeWave(soundSession, *p);
            p++;
        } while (p < g_waveHandles + 64);
        WaveMixCloseChannel(soundSession, 8, 1);
        WaveMixCloseSession(soundSession);
        soundSession = 0;
    }
    if (paDragSaveBits != 0)
        delete paDragSaveBits;
    if (paDragSaveBits2 != 0)
        delete paDragSaveBits2;
    if (pDragTileCanvas != 0)
        delete pDragTileCanvas;
}
// Compiler-generated (GameView vtable @0x44b638 slot 1):
// FUNCTION: YODA 0x00408c40 (??_GGameView scalar-deleting dtor)

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00408df0
// GameView::OnActivateView — on deactivate, cancel any in-progress inventory drag
// (only while in play mode, nFrameMode==4); track the active flag; chain to base.
void CDeskcppView::OnActivateView(BOOL bActivate, CView *pActivateView, CView *pDeactiveView)
{
    switch (bActivate)
    {
    case 0:
        if (pWorld->nFrameMode == 4 && draggedTile != 0)
        {
            bDragActive = 0;
            UpdateDragCursor(1);
            DrawText(0);
            pWorld->nFrameMode = 3;
        }
        bViewActive = 0;
        break;
    case 1:
        bViewActive = 1;
        break;
    }
    CView::OnActivateView(bActivate, pActivateView, pDeactiveView);
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00408e70
// GameView::OnUpdate — partial-repaint dispatcher keyed on lHint: 0=full RedrawWindow,
// 1=item text, 2=weapon box+icon, 3=repaint the game area through a temp DC, 4=health
// dial+needle, 399=first-time init (bind doc, start sound+music, allocate the two
// drag-save bit buffers sized to the screen depth — OOM aborts via THROW_LAST/AfxAbort).
// EFFECTIVE: the case-399 body + the hand-expanded TRY/CATCH_ALL(e)+THROW_LAST+dead-AfxAbort
//   OOM path (engine-bug #7) are BYTE-IDENTICAL to the original; the dispatch cases and all
//   calls match too. Residual is the block-layout open problem: the original sinks the single
//   function epilogue to the physical END (after the shared ReleaseDC tail) and jmps every case
//   to it, while our compile emits the epilogue right after case 0 and cross-jumps the rest —
//   which also shifts how the shared ReleaseDC tail is materialized. Same unmapped mechanism as
//   WorldDoc::GetLocatorIcon; not source-steerable (flat if/else-if vs nested both ~285). G1.
void CDeskcppView::OnUpdate(CView *pSender, LPARAM lHint, CObject *pHint)
{
    if (lHint == 0)
    {
        RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    }
    else if (lHint == 1)
    {
        DrawText(0);
    }
    else if (lHint == 2)
    {
        DrawWeaponBox(0);
        DrawWeaponIcon(0);
    }
    else
    {
        HWND hWnd;
        HDC  hdc;
        if (lHint == 3)
        {
            hdc = ::GetDC(m_hWnd);
            CDC *pDC = CDC::FromHandle(hdc);
            CPalette *pOld = pDC->SelectPalette(pWorld->pPalette, FALSE);
            DrawGameArea(pDC);
            pDC->SelectPalette(pOld, FALSE);
            hdc = pDC->m_hDC;
            hWnd = m_hWnd;
        }
        else
        {
            if (lHint == 4)
            {
                DrawHealthDial(0);
                DrawHealthNeedle(0);
                return;
            }
            if (lHint != 399)
                return;
            if (pWorld == 0)
                pWorld = (CDeskcppDoc *)m_pDocument;
            if (soundSession == 0)
            {
                SoundInit();
                if (pWorld->nMusicEnabled == 1)
                    PlaySound(0x3d);
            }
            nTransitionStep = 0;
            HDC hdcScreen = ::GetDC(NULL);
            if (hdcScreen == 0)
                return;
            int nBytes = GetDeviceCaps(hdcScreen, BITSPIXEL) / 8 << 10;
            TRY {
                paDragSaveBits = new BYTE[nBytes];
                paDragSaveBits2 = new BYTE[nBytes];
            }
            }
            catch (CException *e)
            {
                _afxExceptionLink.m_pException = e;
                AfxMessageBox(0xe01e, 0, (UINT)-1);
                THROW_LAST();
                AfxAbort();                        // sic: dead code after THROW_LAST (#7)
            }
            }
            hWnd = NULL;
            hdc = hdcScreen;
        }
        ::ReleaseDC(hWnd, hdc);
    }
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409060
// GameView::PlaySound — gate the effect by sound/music-enabled flags (ids 0x37 & 0x3a-0x3f
// are music, gated by nMusicEnabled; others by nSoundEnabled; id 3 muted while the walk-sound
// suppression flag is set) then submit it to WAVMIX32. The wave handle comes from the loaded
// table; ids >= 0x40 map only 0x25 (else lpMixWave is left uninitialized — sic, engine quirk).
// EFFECTIVE (48/48 insns, byte_diff 21): logic + MIXPLAYPARAMS layout exact. Residual is a
//   register-allocation tie-break — the original keeps nSoundEnabled in EAX (reused by the
//   else-branch sound gate) and pWorld in EDX; our compile assigns them the opposite registers,
//   a consistent bijection that propagates (identity_miss=9). Not source-steerable (goto/epilogue
//   variants gave the identical score); a TU-phase reg-alloc residual, G1 fodder.
#ifdef GAME_INDY
// GAME_INDY: shared engine code passes Yoda sound ids — translate them (silently dropping
// Yoda-only concepts); data-driven (Indy-native) ids enter below via PlaySoundData.
void CDeskcppView::PlaySound(int nSoundId)
{
    nSoundId = Indy_MapSoundId(nSoundId);
    if (nSoundId >= 0)
        PlaySoundData(nSoundId);
}

void CDeskcppView::PlaySoundData(int nSoundId)
#else
void CDeskcppView::PlaySound(int nSoundId)
#endif
{
    MIXPLAYPARAMS mix;
    CDeskcppDoc *pW = pWorld;
    if (pW->nSoundEnabled == 0 && pW->nMusicEnabled == 0)
        return;
    if (nSoundId == 3 && bSuppressWalkSound != 0)
        return;
#ifdef GAME_INDY
    // Indy music ids are MIDI sequences via MCI, not WaveMix (DESKADV FUN_1010_e43c:
    // id > 0xd && id != the hardcoded eep wave).
    if (nSoundId >= SND_INDY_FLOURISH && nSoundId <= SND_INDY_EERIE)
    {
        if (pW->nMusicEnabled != 0)
            Indy_MidiPlay(nSoundId);
        return;
    }
    if (pW->nSoundEnabled == 0)
        return;
#else
    if (nSoundId == 0x37 || (nSoundId >= 0x3a && nSoundId <= 0x3f))
    {
        if (pW->nMusicEnabled == 0)
            return;
    }
    else if (pW->nSoundEnabled == 0)
        return;
#endif
    int session = soundSession;
    if (session == 0)
        return;
    mix.hMixSession = session;
    mix.iChannel = 0;
    mix.wSize = 0x18;
    if (nSoundId < 0x40)
        mix.lpMixWave = g_waveHandles[nSoundId];
    else if (nSoundId == 0x25)
        mix.lpMixWave = g_waveHandles[5];
    mix.wReserved = 0;
    mix.dwFlags = 2;
    mix.hWndNotify = 0;
    WaveMixPlay(&mix);
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409110
// GameView::OnDraw — the main frame paint: lazy-load the .wld save state once, select the
// world palette, bevel-border the viewport/inventory/scrollbar rects, draw the HUD (health
// dial+needle, direction arrows, item text, weapon box+icon), blit the 288x288 canvas, then
// on the very first paint lazy-run World::Load (the .dta worldgen) — aborting on failure.
// EFFECTIVE (logic/structure faithful; the World::Load tail is byte-identical to the original):
//   our compile hoists the constant 0 into EDI (reused across the ~7 `push 0`/FALSE arg sites:
//   SelectPalette x2, DrawRect bRaised x3, BitBlt srcX/srcY), which pulls EBP in as a 4th
//   callee-saved register (`push ebp`) and cascades a register-rename through the middle. The
//   original pushes 0 immediates and uses only EBX/ESI/EDI. Pure enregistration-heuristic
//   tie-break (lesson #7/#8) — not source-steerable; expected to settle in G1.
void CDeskcppView::OnDraw(CDC *pDC)
{
    CRect rc;
    if (pWorld == 0)
        pWorld = (CDeskcppDoc *)m_pDocument;
    if (pWorld == 0)
        return;
    if (pWorld->bStateFileLoaded == 0)
    {
        pWorld->bStateFileLoaded++;
        pWorld->LoadWorldStateFile();
    }
    CPalette *pOldPalette = pDC->SelectPalette(pWorld->pPalette, FALSE);
    RealizePalette(pDC->m_hDC);
    GetClientRect(&rc);
    rc.left = pWorld->rectUnk3274.left - 3;
    rc.top = pWorld->rectUnk3274.top - 3;
    rc.right = pWorld->rectUnk3274.right + 3;
    rc.bottom = pWorld->rectUnk3274.bottom + 3;
    pWorld->DrawRect(pDC, &rc, 0, 3);
    rc.left = pWorld->rectUnk3284.left - 2;
    rc.right = pWorld->rectUnk3284.right + 2;
    rc.top = pWorld->rectUnk3284.top - 2;
    rc.bottom = pWorld->rectUnk3284.bottom + 2;
    pWorld->DrawRect(pDC, &rc, 0, 2);
    rc.left = pWorld->rectInvScroll.left - 2;
    rc.right = pWorld->rectInvScroll.right + 2;
    rc.top = pWorld->rectInvScroll.top - 2;
    rc.bottom = pWorld->rectInvScroll.bottom + 2;
    pWorld->DrawRect(pDC, &rc, 0, 2);
    DrawHealthDial(pDC);
    DrawDirectionArrows(pDC);
    DrawHealthNeedle(pDC);
    DrawText(pDC);
    DrawWeaponBox(pDC);
    DrawWeaponIcon(pDC);
    if (pWorld->pCanvas != 0)
    {
        int srcX, srcY;
        if (bMapAtCanvasOriginMaybe == 1)
        {
            srcX = 0;
            srcY = 0;
        }
        else
        {
            srcY = pWorld->nViewTop;
            srcX = pWorld->nViewLeft;
        }
        pWorld->pCanvas->BitBlt(pDC, pWorld->rectUnk3274.left, pWorld->rectUnk3274.top,
                                0x120, 0x120, srcX, srcY);
        pDC->SelectPalette(pOldPalette, FALSE);
        if (pWorld != 0 && pWorld->bDtaLoaded == 0)
        {
            pWorld->nFrameMode = 7;
            pWorld->nMapChangeReason = 1;
            bBusy = 0;
            pWorld->bDtaLoaded++;
            if (pWorld->Load() == 0)
            {
                AfxMessageBox(0xe01d, 0x10, (UINT)-1);
                pWorld->OnCloseDocument();
                AfxAbort();
            }
        }
    }
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409340
// InvScrollBar::OnHScroll — the inventory bar reuses its vertical handler for horizontal
// scroll (GameView::OnHScroll forwards WM_HSCROLL here). Plain tail-forward (no frame).
void InvScrollBar::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
    OnVScroll(nSBCode, nPos, pScrollBar);
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409360
// InvScrollBar::OnVScroll — reflected scroll handler; adjusts scrollPos per the
// SB_* code (clamped to [0, scrollMax]) then repaints the parent view's item list.
// EFFECTIVE-WIP (align~144): structure is faithful to the decompile — the max-clamp
//   `scrollPos = nMax` is ONE shared block living in case SB_LINEDOWN, reached by fall-
//   through there and `goto clampMax` from SB_PAGEDOWN + SB_THUMB*. Two reg-alloc/instr-
//   selection residuals, both TU-phase-sensitive (lesson #6/#7, expect G1 to resolve):
//   (a) our compile caches &scrollPos in EDX (`lea edx,[esi+0x40]`) and accesses [edx],
//       while the original keeps `this` in ESI and accesses [esi+0x40] directly — a pure
//       register-allocation choice driven by scrollPos's use-count across all arms + tail;
//   (b) the max-clamp compare emits `cmp;jle` vs the original's `cmp;jge` (operand order,
//       the canonicalized cmp knob MSVC picks internally — proven not source-steerable).
void InvScrollBar::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
    BOOL bChanged;
    int  nMax;
    int  scrl[2];

    int nNew;
    if (this != (InvScrollBar *)pScrollBar)
        return;
    bChanged = FALSE;
    switch (nSBCode)
    {
    case SB_LINEUP:
        nNew = scrollPos - 1;
        scrollPos = nNew;
        if (nNew < 0)
            scrollPos = 0;
        break;
    case SB_LINEDOWN:
        nMax = scrollMax;
        nNew = scrollPos + 1;
        scrollPos = nNew;
        if (nMax < nNew)
        {
clampMax:
            scrollPos = nMax;
        }
        break;
    case SB_PAGEUP:
        nNew = scrollPos - 7;
        scrollPos = nNew;
        if (nNew < 0)
            scrollPos = 0;
        break;
    case SB_PAGEDOWN:
        nMax = scrollMax;
        nNew = scrollPos + 7;
        scrollPos = nNew;
        if (nMax < nNew)
            goto clampMax;
        break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
        scrollPos = nPos;
        if ((int)nPos < 0)
            scrollPos = 0;
        nMax = scrollMax;
        if (nMax < scrollPos)
            goto clampMax;
        break;
    default:
        goto skip;
    }
    bChanged = TRUE;
skip:
    if (bChanged)
    {
        ::GetScrollPos(m_hWnd, SB_CTL);
        ::GetScrollRange(m_hWnd, SB_CTL, &scrl[0], &scrl[1]);
        ::SetScrollPos(m_hWnd, SB_CTL, scrollPos, TRUE);
        CDeskcppView *pView = (CDeskcppView *)CWnd::FromHandle(::GetParent(m_hWnd));
        if (pView != 0)
            pView->DrawText(0);
    }
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409460
// GameView::DrawZoneCell — composite one map cell (zone coords x,y) into the back-
// buffer canvas. Layer 0 is the opaque ground (always BlitFast); layers 1 & 2 are
// the overlay tiles (masked blit when the tile's transparency flag @+0x404 bit0 is
// set, otherwise opaque). Bounds-guarded against the current zone's extent — note
// the asymmetry the 1997 code has: valid x is [0,width) but valid y is [0,height].
// void return: every guard is a bare `return;` and no epilogue sets EAX (Ghidra's
// "int"/longlong return + phantom param_2 were __thiscall ABI-confusion artifacts).
//
// Register/CSE notes for matching: the guard caches pWorld->currentZone (ECX) and the
// LAYER-0 GetTile reuses that cached pointer, while layers 1 & 2 reload
// pWorld->currentZone fresh — hence pZone-> for layer 0 vs pWorld->currentZone-> for
// 1 & 2. x<<5 / y<<5 (the destination pixel coords, 16-bit because destX/destY are
// short) are hoisted once into ESI/EBX and shared across all three layers' blits.
void CDeskcppView::DrawZoneCell(short x, short y)
{
    Tile *pTile;
    short sx, sy;

    // currentZone is evaluated lazily inside the guard (after x<0) and CSE'd across the
    // width/height tests AND layer 0's GetTile — one load held in a register; layers 1 & 2
    // reload it because the intervening blit calls clobber the caller-saved register.
    if (x < 0 || pWorld->currentZone->width <= x ||
        y < 0 || pWorld->currentZone->height < y)
        return;
    sx = x << 5;                     // destination pixel coords, held persistently
    sy = y << 5;
    // layer 0 — opaque ground (tile id is a signed short: -1 = empty ⇒ movsx)
    pTile = pWorld->GetTileData((short)pWorld->currentZone->GetTile(x, y, 0));
    if (pTile != 0)
        pWorld->pCanvas->BlitFast(pTile->pixels, 0x20, 0x20, 0x20, sx, sy);
    // layer 1 — overlay
    pTile = pWorld->GetTileData((short)pWorld->currentZone->GetTile(x, y, 1));
    if (pTile != 0)
    {
        if (pTile->flags & 1)
            pWorld->pCanvas->BlitMasked((char *)pTile->pixels, 0x20, 0x20, sx, sy, 0);
        else
            pWorld->pCanvas->BlitFast(pTile->pixels, 0x20, 0x20, 0x20, sx, sy);
    }
    // layer 2 — overlay
    pTile = pWorld->GetTileData((short)pWorld->currentZone->GetTile(x, y, 2));
    if (pTile != 0)
    {
        if (pTile->flags & 1)
            pWorld->pCanvas->BlitMasked((char *)pTile->pixels, 0x20, 0x20, sx, sy, 0);
        else
            pWorld->pCanvas->BlitFast(pTile->pixels, 0x20, 0x20, 0x20, sx, sy);
    }
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x004095d0
// GameView::DrawZoneCellRect — repaint the rectangular block of cells [x1..x2] ×
// [y1..y2] (inclusive), y-outer / x-inner. int args pass straight to DrawZoneCell's
// short params (the compiler pushes the full dword; the callee reads the low word).
// EFFECTIVE (align=22, 27/27 insns, control flow identical): the residual is a pure
//   register-role ROTATION — the original assigns this→EBP (least-used ⇒ last-choice
//   reg), y→EDI, x2→EBX, x→ESI; ours rotates them differently, which reschedules the
//   4 callee-saved pushes and the incoming-arg loads (hence the [esp+0x18] vs [esp+
//   0x10] offset drift). Plus the compiler-emitted inner-loop entry guard picks
//   `cmp x,x2; jg` (orig) vs our `cmp x2,x; jl` — the canonicalized cmp-direction knob
//   MSVC selects internally (lesson #6, proven not source-steerable). No source shape
//   flips `this` out of the last-choice register here; a G1 permuter/dial pass resolves
//   it. Decl order x,y↔y,x only trades reg_pen (both stay align=22).
void CDeskcppView::DrawZoneCellRect(int x1, int y1, int x2, int y2)
{
    int y, x;

    for (y = y1; y <= y2; y++)
        for (x = x1; x <= x2; x++)
            DrawZoneCell(x, y);
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409610
// GameView::DrawWholeZone — repaint every cell of the current zone, back-to-front
// (both loops count down from width-1). Uses the zone WIDTH for both axes; zones are
// square (18×18) so this covers the whole grid. width-1 is hoisted into EBP and
// copied into the outer (y) counter + reset into the inner (x) counter each row.
// EFFECTIVE (align=0, reg_pen=4, 27/27 insns): structure is byte-exact; the sole
//   residual is the outer(y)/inner(x) counters landing in swapped registers (orig
//   y→EBX x→ESI, ours the reverse). This function was BYTE-EXACT under the prior dial
//   (before DrawZoneCell's currentZone-CSE form landed) — it is now PHASE-DISPLACED by
//   the very edit that made DrawZoneCell exact (they can't both be exact under one
//   global dial; the 361-byte DrawZoneCell wins). Decl-order permutations (x,y / y,x /
//   m-first) only trade reg_pen; none re-flip it. G1 joint pass resolves it.
void CDeskcppView::DrawWholeZone()
{
    int x, y;
    int m = pWorld->currentZone->width - 1;

    for (y = m; y >= 0; y--)
        for (x = m; x >= 0; x--)
            DrawZoneCell(x, y);
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409650
// GameView::ZoneTransitionStep — one step of the square-wipe zone transition,
// driven by OnTimer frame-mode 6 (nStep 0..10):
//   steps 0..4  — paint one shrinking ring of black 32x32 tiles into the canvas
//                 (top/bottom/left/right edges per iteration), then blit the whole
//                 288x288 viewport to the screen;
//   step  5     — the actual switch: currentZone = GetZoneById(nZoneId) (restoring
//                 the old zone if the id is bad), locate the new zone in the 10x10
//                 map grid (world-map player position + ZONE_TYPE 7 flagA), run
//                 IACT zone-entry triggers 4 and 5 (bIactZoneEntryMaybe wraps
//                 trigger 5), set frame mode 6 (or keep 0xb), DrawEntities +
//                 DrawWholeZone, mark the grid cell visited (flagSolved);
//   steps 6..9  — blit an expanding centered square of the (redrawn) viewport;
//   step 10     — DrawDirectionArrows + a final IACT trigger 5 (skipped when
//                 bSkipEntryIactMaybe), frame mode 6 / kept.
// Returns 0 when trigger 5 at step 5 reported a warp (0x800) that trigger 4 hadn't
// already claimed (caller restarts the transition); else 1.
// The black source tile is a heap Tile allocated under TRY/CATCH_ALL every call —
// the catch is the recurring dead OOM box (AfxMessageBox(0xe01e)+AfxAbort).
// sic: engine-bugs.md #13 — step 10's nMask is READ UNINITIALIZED when its IactRun
// is skipped (bSkipEntryIactMaybe set or world invalid): the &4 / &0x20 redraw
// tests then act on stack garbage.
//
// EFFECTIVE MATCH (441/441 insns, align=48, reg_pen=7 — ~97% identical; the raw
// byte-diff lies here: a 4-byte frame delta shifts every tail EBP offset). Structure
// proven: register roles (i→EBX, sy→EDI, ESI=&pWorld), all four blit sites, both
// loops, the whole step-5/10 logic and the duplicated cleanup arms are insn-identical.
// Residual autopsy (all probed; none source-steerable — G1 fodder):
//  * KEY CRACKS that got here (reuse for WorldEntryStepMaybe): (a) NO `x` local for
//    `sx + i` — the original's slot -0x40 is the compiler's OWN CSE temp (written +
//    reloaded); declaring `short x` promoted it to a register, cascaded sy into a
//    slot, and made the invariant `span + sy` get LICM-hoisted out of the loop
//    (align 188 → 48 from removing it). (b) `span` IS a real local (slot -0x1c),
//    summed at the call site each iteration.
//  * blit4 window: orig emits `movsx ebx,bx` before `add bx,0x20` (maintains (int)i
//    for the `i + sy` leas, base=EBX) and increments BETWEEN the y/x pushes; ours
//    picks lea base=EDI, drops the movsx and increments before the pushes. Probed:
//    sy+i vs i+sy (inert), (int)i+sy (worse), cnt--/i+= order swap (much worse) —
//    IL value-numbering, not source-steerable.
//  * head: orig calls GetDC via `mov ebx,[__imp_GetDC]; call ebx` — UNIQUE in the
//    binary (OnUpdate/DrawDirectionArrows/... all `call [iat]`). Probed: member
//    GetDC()/ReleaseDC(pDC) (identical output — kept), the explicit
//    FromHandle-of-::GetDC spelling (identical), HDC local (worse). The EBX free
//    there is what hands the original nStep→BX at the ladder; ours converges to the
//    same roles by the loop anyway.
//  * 1-insn scheduling drift of `xor di,di` (nMask = 0 emitted before vs after the
//    unk50 store), nSavedMode in DX vs AX, and the 4B-smaller frame (ours packs the
//    cnt/sx2 word slots where the orig dword-spaces them).
int CDeskcppView::ZoneTransitionStep(short nZoneId, short nStep)
{
    Tile *pTile;
    TRY {
        pTile = new Tile;
    }
    }              // closes the try block the TRY macro opened
    catch (CException *e) {                // hand-expanded CATCH_ALL(e)
        _afxExceptionLink.m_pException = e;
        AfxMessageBox(0xe01e, 0, (UINT)-1);    // sic: dead OOM dialog
        AfxAbort();                            //      (docs/engine-bugs.md #7)
    }
    }              // closes the TRY macro's outer (link-scope) brace
    unsigned char *pPixels = pTile->pixels;
    memset(pPixels, 0, 0x400);
    short nCellX = (short)pWorld->playerX;     // world-map grid coords; conditionally
    short nCellY = (short)pWorld->playerY;     // re-derived by step 5's grid search
    CDC *pDC = GetDC();
    CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
    int bAborted = 0;
    if (nStep < 5)
    {
        short sx = (short)pWorld->nViewLeft + nStep * 32;
        short sy = (short)pWorld->nViewTop + nStep * 32;
        short n = 9 - nStep * 2;               // ring edge length, in tiles
        if (n > 0)
        {
            short i = 0;
            short span = (n - 1) * 32;
            short sx2 = span + sx;
            short cnt = n;
            do
            {
                pWorld->pCanvas->BlitFast(pPixels, 0x20, 0x20, 0x20, sx + i, sy);    // top
                pWorld->pCanvas->BlitFast(pPixels, 0x20, 0x20, 0x20, sx + i,
                                          span + sy);                                // bottom
                pWorld->pCanvas->BlitFast(pPixels, 0x20, 0x20, 0x20, sx, i + sy);    // left
                pWorld->pCanvas->BlitFast(pPixels, 0x20, 0x20, 0x20, sx2, i + sy);   // right
                i += 32;
                cnt--;
            } while (cnt != 0);
        }
        pWorld->pCanvas->BitBlt(pDC, pWorld->rectUnk3274.left, pWorld->rectUnk3274.top,
                                0x120, 0x120, pWorld->nViewLeft, pWorld->nViewTop);
    }
    else if (nStep == 5)
    {
        Zone *pOldZone = pWorld->currentZone;
        pWorld->currentZone = pWorld->GetZoneById(nZoneId);
        if (pWorld->currentZone == NULL)
            pWorld->currentZone = pOldZone;
        if (pWorld->bWorldInvalid == 0)
        {
            short y = 0;
            nCellX = -1;
            nCellY = -1;
            do
            {
                short x = 0;
                do
                {
                    if (pWorld->GetZoneById(pWorld->GetZoneCell(x, y)) == pWorld->currentZone)
                    {
                        nCellX = x;
                        nCellY = y;
                        break;
                    }
                    x++;
                } while (x < 10);
                y++;
            } while (y < 10);
            if (nCellX >= 0)
            {
                if (nCellY >= 0)
                {
                    pWorld->playerX = nCellX;
                    pWorld->playerY = nCellY;
                }
                if (nCellX >= 0 && nCellY >= 0 &&
                    pWorld->mapGrid[nCellX + nCellY * 10].zoneType == 7)
                    pWorld->mapGrid[nCellX + nCellY * 10].flagA = 1;
            }
        }
        pWorld->unk50 = nZoneId;
        unsigned short nMask = 0;
        pWorld->UpdateCamera();
        pWorld->RefreshZone();
        short nSavedMode = (short)pWorld->nFrameMode;
        if (pWorld->bWorldInvalid == 0)
        {
            nMask = (unsigned short)pWorld->currentZone->IactRun(4, 0, 0, 0, 0, 0,
                                                                 pDC, pWorld, this);
            if (nMask & 0x800)
                bAborted = 1;
        }
        bIactZoneEntryMaybe = 1;
        if (pWorld->bWorldInvalid == 0)
            nMask |= (unsigned short)pWorld->currentZone->IactRun(5, 0, 0, 0, 0, 0,
                                                                  pDC, pWorld, this);
        bIactZoneEntryMaybe = 0;
        if (nMask & 4)
            pWorld->UpdateCamera();
        if (nSavedMode != 0xb)
            pWorld->nFrameMode = 6;
        else
            pWorld->nFrameMode = 0xb;
        pWorld->currentZone->activatedFlag = 1;
        if (pWorld->bWorldInvalid == 0)
            DrawEntities();
        DrawWholeZone();
#ifdef GAME_INDY
        // Entering a building interior (door-in) targets a zone that is NOT in the 10x10
        // overworld grid, so the grid search leaves nCellX/nCellY = -1 and the unguarded Yoda
        // write below hits mapGrid[-11] (an out-of-bounds store into the World struct that can
        // corrupt an adjacent pointer -> a later GDI crash). WorldEntryStepMaybe guards this
        // same store; ZoneTransitionStep (byte-matched to the original) does not. Guard it for
        // Indy only (Yoda fall-through keeps the exact original unguarded store, anchor 211).
        if (nCellX >= 0 && nCellY >= 0)
#endif
        pWorld->mapGrid[nCellX + nCellY * 10].flagSolved = 1;
        if (bAborted == 0 && (nMask & 0x800))
        {
            pDC->SelectPalette(pOldPal, 0);
            ReleaseDC(pDC);
            delete pTile;
            return 0;
        }
    }
    else if (nStep == 10)
    {
        DrawDirectionArrows(pDC);
        short nSavedMode = (short)pWorld->nFrameMode;
        unsigned short nMask; YODA_SIC_FIX(nMask = 0;) // sic: uninitialized when the IactRun
                                               //      below is skipped (engine-bugs #13)
        if (bSkipEntryIactMaybe == 0 && pWorld->bWorldInvalid == 0)
            nMask = (unsigned short)pWorld->currentZone->IactRun(5, 0, 0, 0, 0, 0,
                                                                 pDC, pWorld, this); YODA_SIC_FIX(else BUGLOG(("sic#13 ZoneTransitionStep: entry IactRun skipped, mask defaulted to 0\n"));)
        if (nMask & 4)
            pWorld->UpdateCamera();
        bSkipEntryIactMaybe = 0;
        if (nSavedMode != 0xb)
            pWorld->nFrameMode = 6;
        else
            pWorld->nFrameMode = nSavedMode;
        if (nMask & 0x20)
            DrawWholeZone();
    }
    else
    {
        short nOff = (10 - nStep) * 32;
        short nSize = (nStep - 1) * 32;
        pWorld->pCanvas->BitBlt(pDC, pWorld->rectUnk3274.left + nOff,
                                pWorld->rectUnk3274.top + nOff, nSize - nOff, nSize - nOff,
                                pWorld->nViewLeft + nOff, pWorld->nViewTop + nOff);
    }
    pDC->SelectPalette(pOldPal, 0);
    ReleaseDC(pDC);
    delete pTile;
    return 1;
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409c10
// GameView::WorldEntryStepMaybe — twin of ZoneTransitionStep, driven by OnTimer
// frame-mode 0xb (fresh world entry): steps 0..4 are NO-OPS (no black ring — the
// whole body sits under `if (nStep >= 5)`); step 5 = the same zone switch/IACT 4+5
// sequence but with nTransitionStep = -1 instead of the mode save/restore, and the
// grid-cell visited mark GUARDED by the found coords (unlike ZoneTransitionStep's
// unguarded store); steps 6..9 = the expanding reveal blit; step 10 = direction
// arrows + final IACT 5, then chains into mode 6 (nMapChangeReason = 1,
// nTransitionStep = 0). Returns 0 on an unclaimed warp (0x800) from trigger 5.
// sic: engine-bugs.md #13 (same bug as ZoneTransitionStep): step 10's nMask is
// read uninitialized when its IactRun is skipped — here the compiler even emits an
// explicit `mov di,[ebp-0x18]` load of the never-written slot on the skip path.
// NOTE the head here vindicates ZoneTransitionStep's parked head residuals: THIS
// function's original calls GetDC directly via `call [__imp_GetDC]`, loads nStep
// into DI and pWorld into EAX (cx/ax word loads) — exactly the shapes our compiler
// produces for both functions. The memset has no pixel-pointer local (nothing
// reuses it — no ring blits), so it's written inline here.
//
// EFFECTIVE MATCH (349/349 insns, align=8, reg_pen=14 — 13 diff rows, ALL register-
// role). The residual is a clean PARITY CROSSING with ZoneTransitionStep: for the
// same two code shapes (head pWorld/word-load regs: EAX+cx/ax vs ECX+ax/dx; grid
// search: y→EBX,x→DI vs y→EDI,x→BX) the ORIGINAL uses shape A here and shape B in
// ZoneTransitionStep, while OUR compile emits them exactly swapped — both shapes
// reproduce, on the wrong functions. Same TU-phase drift family as the original's
// own loader jg/jl/jg triplet; almost certainly one dial state with the
// DrawZoneCellRect/DrawWholeZone rotations. Do not grind — G1 joint pass.
int CDeskcppView::WorldEntryStepMaybe(short nZoneId, short nStep)
{
    Tile *pTile;
    TRY {
        pTile = new Tile;
    }
    }              // closes the try block the TRY macro opened
    catch (CException *e) {                // hand-expanded CATCH_ALL(e)
        _afxExceptionLink.m_pException = e;
        AfxMessageBox(0xe01e, 0, (UINT)-1);    // sic: dead OOM dialog
        AfxAbort();                            //      (docs/engine-bugs.md #7)
    }
    }              // closes the TRY macro's outer (link-scope) brace
    memset(pTile->pixels, 0, 0x400);
    short nCellX = (short)pWorld->playerX;     // world-map grid coords; conditionally
    short nCellY = (short)pWorld->playerY;     // re-derived by step 5's grid search
    CDC *pDC = GetDC();
    CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
    int bAborted = 0;
    if (nStep >= 5)
    {
        if (nStep == 5)
        {
            Zone *pOldZone = pWorld->currentZone;
            pWorld->currentZone = pWorld->GetZoneById(nZoneId);
            if (pWorld->currentZone == NULL)
                pWorld->currentZone = pOldZone;
            if (pWorld->bWorldInvalid == 0)
            {
                short y = 0;
                nCellX = -1;
                nCellY = -1;
                do
                {
                    short x = 0;
                    do
                    {
                        if (pWorld->GetZoneById(pWorld->GetZoneCell(x, y)) == pWorld->currentZone)
                        {
                            nCellX = x;
                            nCellY = y;
                            break;
                        }
                        x++;
                    } while (x < 10);
                    y++;
                } while (y < 10);
                if (nCellX >= 0)
                {
                    if (nCellY >= 0)
                    {
                        pWorld->playerX = nCellX;
                        pWorld->playerY = nCellY;
                    }
                    if (nCellX >= 0 && nCellY >= 0 &&
                        pWorld->mapGrid[nCellX + nCellY * 10].zoneType == 7)
                        pWorld->mapGrid[nCellX + nCellY * 10].flagA = 1;
                }
            }
            unsigned short nMask = 0;
            pWorld->unk50 = nZoneId;
            pWorld->UpdateCamera();
            pWorld->RefreshZone();
            if (pWorld->bWorldInvalid == 0)
            {
                nMask = (unsigned short)pWorld->currentZone->IactRun(4, 0, 0, 0, 0, 0,
                                                                     pDC, pWorld, this);
                if (nMask & 0x800)
                    bAborted = 1;
            }
            bIactZoneEntryMaybe = 1;
            if (pWorld->bWorldInvalid == 0)
                nMask |= (unsigned short)pWorld->currentZone->IactRun(5, 0, 0, 0, 0, 0,
                                                                      pDC, pWorld, this);
            bIactZoneEntryMaybe = 0;
            if (nMask & 4)
                pWorld->UpdateCamera();
            nTransitionStep = -1;
            pWorld->currentZone->activatedFlag = 1;
            if (pWorld->bWorldInvalid == 0)
                DrawEntities();
            DrawWholeZone();
            if (nCellY >= 0 && nCellX >= 0)
                pWorld->mapGrid[nCellX + nCellY * 10].flagSolved = 1;
            if (bAborted == 0 && (nMask & 0x800))
            {
                pDC->SelectPalette(pOldPal, 0);
                ReleaseDC(pDC);
                delete pTile;
                return 0;
            }
        }
        else if (nStep == 10)
        {
            DrawDirectionArrows(pDC);
            unsigned short nMask; YODA_SIC_FIX(nMask = 0;) // sic: uninitialized when the IactRun
                                               //      below is skipped (engine-bugs #13)
            if (bSkipEntryIactMaybe == 0 && pWorld->bWorldInvalid == 0)
                nMask = (unsigned short)pWorld->currentZone->IactRun(5, 0, 0, 0, 0, 0,
                                                                     pDC, pWorld, this); YODA_SIC_FIX(else BUGLOG(("sic#13 OnTimer step10: entry IactRun skipped, mask defaulted to 0\n"));)
            if (nMask & 4)
                pWorld->UpdateCamera();
            bSkipEntryIactMaybe = 0;
            pWorld->nFrameMode = 6;
            pWorld->nMapChangeReason = 1;
            nTransitionStep = 0;
            if (nMask & 0x20)
                DrawWholeZone();
        }
        else
        {
            short nOff = (10 - nStep) * 32;
            short nSize = (nStep - 1) * 32;
            pWorld->pCanvas->BitBlt(pDC, pWorld->rectUnk3274.left + nOff,
                                    pWorld->rectUnk3274.top + nOff, nSize - nOff,
                                    nSize - nOff, pWorld->nViewLeft + nOff,
                                    pWorld->nViewTop + nOff);
        }
    }
    pDC->SelectPalette(pOldPal, 0);
    ReleaseDC(pDC);
    delete pTile;
    return 1;
}

// =============================================================================
// MFC GDI COMDAT copies emitted into this TU by CBitmap/CBitmapButton member
// usage (this .cpp instantiates CBitmap locals + the btnDialog* members) — all
// six byte-match the originals with zero source lines. Ghidra's
// "CBitmap_CtorMaybe" @0x40a0c0 is really CGdiObject::CGdiObject (rename queued).
// The CBitmap trio sits later in .text (0x40a4e0-0x40a5d0, after DrawTileAt).
// =============================================================================
// FUNCTION: YODA 0x0040a0c0  (??0CGdiObject@@ ctor COMDAT)
// FUNCTION: YODA 0x0040a120  (??_GCGdiObject@@ scalar-deleting dtor COMDAT)
// FUNCTION: YODA 0x0040a1a0  (??1CGdiObject@@ dtor COMDAT)
// FUNCTION: YODA 0x0040a4e0  (??0CBitmap@@ ctor COMDAT)
// FUNCTION: YODA 0x0040a560  (??_GCBitmap@@ scalar-deleting dtor COMDAT)
// FUNCTION: YODA 0x0040a5d0  (??1CBitmap@@ dtor COMDAT)

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x0040a200
// GameView::DrawGameArea — blit the 288x288 game viewport to the screen at client
// (8,7). With no DC supplied, grabs one (with palette) and releases it after. The
// GetPixel probe at (0x138,0x11c) — just outside the viewport — detects the frame
// having been painted over (e.g. by a dialog): if that pixel is valid and no longer
// COLOR_3DFACE, the whole window is invalidated first. Locator-map modes (mode 5
// with the map open, mode 7, or the map open at all — the last || term makes the
// mode-5 check redundant, faithful dev code) blit the canvas from its origin
// (the map is composited at 0,0); play modes blit the camera window (nViewLeft/Top).
void CDeskcppView::DrawGameArea(CDC *pDC)
{
    CPalette *pOldPal = 0;
    if (pDC == 0)
    {
        pDC = GetDC();
        if (pDC == 0)
            return;
        pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
    }
    int nMode = pWorld->nFrameMode;
    COLORREF pixel = ::GetPixel(pDC->m_hDC, 0x138, 0x11c);
    DWORD clr = ::GetSysColor(0xf);
    if (pixel != 0xffffffff && clr != pixel)
        ::RedrawWindow(m_hWnd, 0, 0, 0x105);
    if ((nMode == 5 && bMapViewOpen != 0) || nMode == 7 || bMapViewOpen != 0)
        pWorld->pCanvas->BitBlt(pDC, 8, 7, 0x120, 0x120, 0, 0);
    else
        pWorld->pCanvas->BitBlt(pDC, 8, 7, 0x120, 0x120, pWorld->nViewLeft,
                                pWorld->nViewTop);
    if (pOldPal != 0)
    {
        pDC->SelectPalette(pOldPal, 0);
        ReleaseDC(pDC);
    }
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x0040a320
// GameView::BlitTile — draw one 32x32 tile into the back-buffer canvas at zone
// cell (x,y) (pixel x<<5, y<<5), masked when the tile's transparency flag is set.
// Bounds use the zone WIDTH for both axes (square zones). The 3rd param is passed
// by every caller but never read (byte-proven: RET 0x10 = 4 dword params, no
// [esp+0x14] access) — the header's "(sig?)" tag can come off.
// EFFECTIVE (47/47 insns, align=20): one consistent register rotation (orig x→SI
//   y→DX w→AX; ours x→DX y→AX w→SI) + the induced cmp mirror (`cmp w,x; jle` vs
//   `cmp x,w; jge` — `w > x` and `x < w` spellings compile identically, lesson #6).
//   Crack that mattered: sx/sy MUST be locals — written at the call sites the
//   x<<5/y<<5 pair gets re-emitted per arm instead of hoisted above the flags test.
void CDeskcppView::BlitTile(short y, short x, int nUnused, Tile *pTile)
{
    Canvas *pCanvas = pWorld->pCanvas;
    if (pCanvas != 0 && pTile != 0)
    {
        short w = pWorld->currentZone->width;
        if (x >= 0 && y >= 0 && x < w && y < w)
        {
            short sx = x << 5;
            short sy = y << 5;
            if (pTile->flags & 1)
            {
                pCanvas->BlitMasked((char *)pTile->pixels, 0x20, 0x20, sx, sy, 0);
                return;
            }
            pCanvas->BlitFast(pTile->pixels, 0x20, 0x20, 0x20, sx, sy);
        }
    }
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x0040a3a0
// GameView::DrawTileAt — redraw zone cell (x,y) into the canvas: frame < 0 paints
// all 3 layers back-to-front (the DrawZoneCell composite, but via the raw tile
// array); frame 0..2 paints that single layer only. Tile ids are signed shorts
// (-1 = empty). NOTE the register split the decompile confirms: the 3-layer loop
// re-mentions pWorld->pCanvas / pWorld->tiles fresh each iteration (the calls
// clobber the caller-saved copies), while the single-frame arm reuses the pCanvas
// local and the entry pWorld — write it exactly that way.
// EFFECTIVE (109/109 insns, align=32): pure two-register swap (orig this→EBP,
//   pCell→EBX; ours the reverse) + the same cmp mirror as BlitTile + a 1-insn
//   entry scheduling shuffle. Structure, both arms, the 3-layer countdown loop and
//   every call site are insn-identical. DrawZoneCellRect rotation family — G1.
void CDeskcppView::DrawTileAt(short x, short y, short frame)
{
    Canvas *pCanvas = pWorld->pCanvas;
    if (pCanvas != 0)
    {
        short *pCell = &pWorld->currentZone->tiles[(y * 18 + x) * 3];
        short w = pWorld->currentZone->width;
        if (x >= 0 && y >= 0 && w > x && w > y)
        {
            short sx = x << 5;
            short sy = y << 5;
            if (frame < 0)
            {
                int n = 3;
                do
                {
                    if (*pCell >= 0)
                    {
                        Tile *pTile = (Tile *)pWorld->tiles.GetAt(*pCell);
                        if (pTile->flags & 1)
                            pWorld->pCanvas->BlitMasked((char *)pTile->pixels, 0x20, 0x20,
                                                        sx, sy, 0);
                        else
                            pWorld->pCanvas->BlitFast(pTile->pixels, 0x20, 0x20, 0x20,
                                                      sx, sy);
                    }
                    pCell++;
                    n--;
                } while (n != 0);
            }
            else if (pCell[frame] >= 0)
            {
                Tile *pTile = (Tile *)pWorld->tiles.GetAt(pCell[frame]);
                if (pTile->flags & 1)
                {
                    pCanvas->BlitMasked((char *)pTile->pixels, 0x20, 0x20, sx, sy, 0);
                    return;
                }
                pCanvas->BlitFast(pTile->pixels, 0x20, 0x20, 0x20, sx, sy);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x0040a620
// GameView::IsUsableTileMaybe — hardcoded tile-id predicate: 0 for the special-
// behaviour tile ranges below, 1 for everything else. Written as a switch (66
// cases after range folding) — VC4.2 builds the balanced comparison tree with
// per-subtree copies of the default `return 1` epilogue (the ReadZone whitelist
// pattern, lesson #11).
int CDeskcppView::IsUsableTileMaybe(short tileId)
{
    switch (tileId)
    {
    case 0x67: case 0x68: case 0x69:
    case 0x6b:
    case 0x163:
    case 0x176: case 0x177: case 0x178:
    case 0x17c: case 0x17d: case 0x17e: case 0x17f: case 0x180: case 0x181:
    case 0x182: case 0x183: case 0x184: case 0x185: case 0x186:
    case 0x271: case 0x272:
    case 0x346: case 0x347: case 0x348: case 0x349: case 0x34a: case 0x34b:
    case 0x34c: case 0x34d: case 0x34e: case 0x34f: case 0x350: case 0x351:
    case 0x352: case 0x353: case 0x354: case 0x355: case 0x356: case 0x357:
    case 0x396: case 0x397: case 0x398:
    case 0x39b:
    case 0x39d: case 0x39e: case 0x39f: case 0x3a0: case 0x3a1: case 0x3a2:
    case 0x3a3: case 0x3a4:
    case 0x3fd:
    case 0x47b: case 0x47c: case 0x47d: case 0x47e: case 0x47f: case 0x480:
    case 0x481: case 0x482: case 0x483: case 0x484: case 0x485: case 0x486:
    case 0x53d:
    case 0x740:
        return 0;
    }
    return 1;
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x0040a710
// GameView::FireWeaponStep — one frame of the weapon-fire animation, driven by
// OnTimer mode 8 with nStep = nFireStep 0..3. The player's cell comes from the
// camera (cameraX/32, cameraY/32). Step 0 plays the fire sound and pays ammo
// (weapon tiles 0x1fe "The Force" and 0x12 lightsaber are free; the five ammo
// counters 0x334c.. map to weapon tiles 0x1ff/0x200/0x201/0x204/0x205). Each step
// draws the muzzle tile at the player, the beam tile one cell ahead, and walks
// the projectile tile outward (alternating animation via nStep % 2 + 1); the
// blaster (0x1ff) leaves its impact cell in nWeaponHitX/YMaybe, other weapons
// stop flight (nFireStep = 4) on a usable/blocking overlay tile. After drawing,
// the viewport is flushed and UseWeapon resolves hits; an emptied weapon is
// deselected, removed from inventory, re-armed from a spare in the inventory
// (with per-weapon ammo refill + sound) or falls back to the lightsaber
// character (frames[7] == 0x12).
// NOTE the X (horizontal) and Y (vertical) projectile arms are genuinely
// ASYMMETRIC in the original: the X arm duplicates the draw-triple in all three
// no-stop paths, the Y arm shares one copy behind skip-gotos; the Y arm also
// tests (flags & 0x60000) before IsUsableTileMaybe where the X arm combines
// them. Faithful dev code — do not "clean up".
//
// EFFECTIVE MATCH (820 vs 828 insns, ~97% identical; part of the align score is
// the 7-entry ammo jump-table DATA at the COMDAT end misaligning as insns).
// Structure proven: both dir arms, all draw triples, the erase/beam blocks, the
// dead-weapon rescan loops and the refill switch are insn-identical up to one
// consistent register rotation (this: EBX↔ESI etc.). Cracks that landed it:
// int nWeaponTile local (movsx+dword slot) with the CONDITIONS re-mentioning
// pWeapon->frames[7] (16-bit cmps); nStep == 0 duplicated into BOTH head
// conditions (the binary jump-threads them); t != -1 arms FIRST everywhere;
// the X non-blaster stop test is `IsUsable && bBlocked == 0` with an int
// bBlocked set from a chained GetTileData(t)->flags test; refill cases assign
// pWeapon->unk48 in BOTH if/else arms (cross-jumped); `unk48 <= 0` (zero-reg
// cmp) not `< 1`. Parked residuals (probed, not source-steerable):
//  * OPEN CODEGEN AXIS — the original tests tile flags as `mov eax,[pT+0x404];
//    test eax,0x60000` (4 sites) where every spelling we tried (direct field,
//    pT local, flags local, outer-scope flags, (int) cast, chained call)
//    byte-narrows to `test byte [pT+0x406],6`. Detonate's original DOES use the
//    narrow form for `& 0x20000` — so the wide form here is a real source shape
//    we haven't found. Related: the compiler VN-eliminates our bBlocked into a
//    spilled flags copy at the X non-blaster site.
//  * erase-block `dir * nStep + x`: orig folds nStep as an imul-mem operand and
//    sign-tests via the add (js); ours materializes nStep and uses lea+test.
//  * 1-insn scheduling drifts at the cleanup GetDC head and the two scan-loop
//    heads; backedge cmp directions (jl vs jg — canonicalization, both source
//    spellings identical); jump-table byte differences from reloc masking.
void CDeskcppView::FireWeaponStep(int nStep)
{
    int x = pWorld->cameraX / 32;
    int y = pWorld->cameraY / 32;
    int w = pWorld->currentZone->width;
    int ty = nFireDirY + y;
    int tx = nFireDirX + x;
    Character *pWeapon = pWorld->currentWeapon;
    if (pWeapon == 0)
        return;
    bBusy = 1;
    int nWeaponTile = pWeapon->frames[7];
#ifdef GAME_INDY
    // Indy's whip is a reusable MELEE weapon; guns are the depletable class. DESKADV (RE'd) keys
    // its weapon class off tile flag bit 16 = TILE_LIGHT_BLASTER (it tests NO bit-18/TILE_LIGHTSABER
    // — that was a DesktopAdventures reimpl assumption). So treat any non-blaster weapon tile as
    // reusable (never deplete unk48 / never RemoveItem). Yoda's hardcoded reusable ids (0x12
    // lightsaber / 0x1fe Force) don't apply to Indy's whip tile. Yoda #else = exact original.
    if (nStep == 0 && (nWeaponTile == 0x1fe ||
                       (pWorld->GetTileData(nWeaponTile) != NULL &&
                        !(pWorld->GetTileData(nWeaponTile)->flags & TILE_LIGHT_BLASTER))))
#else
    if (nStep == 0 && (pWeapon->frames[7] == 0x1fe || pWeapon->frames[7] == 0x12))
#endif
    {
        PlaySoundData(pWeapon->weaponCharId);
        DrawWeaponIcon(0);
    }
    else if (nStep == 0 && pWeapon->unk48 > 0)
    {
        PlaySoundData(pWeapon->weaponCharId);
        pWeapon->unk48--;
        DrawWeaponIcon(0);
        pWorld->bWeaponHitPending = 0;
        switch (nWeaponTile)
        {
        case 0x1ff:
            pWorld->weaponState[0]--;
            break;
        case 0x200:
            pWorld->weaponState[1]--;
            break;
        case 0x201:
            pWorld->weaponState[2]--;
            break;
        case 0x204:
            pWorld->weaponState[3]--;
            break;
        case 0x205:
            pWorld->nCurrentAmmoMaybe--;
            break;
        }
    }
    Tile *pBeamH = (Tile *)pWeapon->GetProjectileTile(nStep % 2 + 1, nFireDirX, nFireDirY,
                                                      1, &pWorld->tiles);
    Tile *pBeamV = (Tile *)pWeapon->GetProjectileTile(nStep % 2 + 1, nFireDirX, nFireDirY,
                                                      2, &pWorld->tiles);
    Tile *pProj = (Tile *)pWeapon->GetProjectileTile(nStep % 2 + 1, nFireDirX, nFireDirY,
                                                     0, &pWorld->tiles);
    if (nFireStep < 3)
        DrawTileAt((short)x, (short)y, 0);
    if (nFireDirX != 0)
    {
        if (tx >= 0 && tx < w)
            DrawZoneCell((short)tx, (short)y);
    }
    else if (nFireDirY != 0)
    {
        if (ty >= 0 && ty < w)
            DrawZoneCell((short)x, (short)ty);
    }
    if (nStep < 3)
    {
        BlitTile((short)y, (short)x, 1, pBeamH);
        DrawTileAt((short)x, (short)y, 2);
        if (nFireDirX != 0 && pBeamV != 0)
        {
            if (tx >= 0 && tx < w)
            {
                BlitTile((short)y, (short)tx, 1, pBeamV);
                DrawTileAt((short)tx, (short)y, 2);
            }
        }
        else if (nFireDirY != 0 && pBeamV != 0 && ty >= 0 && ty < w)
        {
            BlitTile((short)ty, (short)x, 1, pBeamV);
            DrawTileAt((short)x, (short)ty, 2);
        }
    }
    if (pProj != 0)
    {
        if (nStep > 0)
        {
            if (nFireDirX != 0)
            {
                int px = nFireDirX * nStep + x;
                if (px >= 0 && w > px)
                    DrawTileAt((short)px, (short)y, -1);
            }
            else if (nFireDirY != 0)
            {
                int py = nFireDirY * nStep + y;
                if (py >= 0 && w > py)
                    DrawTileAt((short)x, (short)py, -1);
            }
        }
        if (nStep >= 0 && nStep < 3)
        {
            int dx = nFireDirX;
            if (dx != 0)
            {
                int nx = (nStep + 1) * dx + x;
                if (nx >= 0 && w > nx)
                {
                    if (pWeapon->frames[7] == 0x1ff)
                    {
                        short t = pWorld->currentZone->GetTile(nx - dx, y, 1);
                        if (t == -1)
                        {
                            DrawTileAt((short)nx, (short)y, 0);
                            DrawTileAt((short)nx, (short)y, 1);
                            BlitTile((short)y, (short)nx, 1, pProj);
                            DrawTileAt((short)nx, (short)y, 2);
                            pWorld->nWeaponHitX = nx;
                            pWorld->nWeaponHitY = y;
                        }
                        else
                        {
                            if ((pWorld->GetTileData(t)->flags & 0x60000) == 0)
                            {
                                DrawTileAt((short)nx, (short)y, 0);
                                DrawTileAt((short)nx, (short)y, 1);
                                BlitTile((short)y, (short)nx, 1, pProj);
                                DrawTileAt((short)nx, (short)y, 2);
                                pWorld->nWeaponHitX = nx;
                                pWorld->nWeaponHitY = y;
                            }
                            else
                            {
                                nFireStep = 4;
                            }
                        }
                    }
                    else
                    {
                        int bBlocked = 0;
                        short t = pWorld->currentZone->GetTile(nx, y, 1);
                        if (t != -1)
                        {
                            if (pWorld->GetTileData(t)->flags & 0x60000)
                                bBlocked = 1;
                            if (IsUsableTileMaybe(t) != 0 && bBlocked == 0)
                            {
                                nFireStep = 4;
                            }
                            else
                            {
                                DrawTileAt((short)nx, (short)y, 0);
                                DrawTileAt((short)nx, (short)y, 1);
                                BlitTile((short)y, (short)nx, 1, pProj);
                                DrawTileAt((short)nx, (short)y, 2);
                            }
                        }
                        else
                        {
                            t = pWorld->currentZone->GetTile(nx - nFireDirX, y, 1);
                            if (t != -1)
                            {
                                if (IsUsableTileMaybe(t) == 0)
                                {
                                    DrawTileAt((short)nx, (short)y, 0);
                                    DrawTileAt((short)nx, (short)y, 1);
                                    BlitTile((short)y, (short)nx, 1, pProj);
                                    DrawTileAt((short)nx, (short)y, 2);
                                }
                                else
                                {
                                    nFireStep = 4;
                                }
                            }
                            else
                            {
                                DrawTileAt((short)nx, (short)y, 0);
                                DrawTileAt((short)nx, (short)y, 1);
                                BlitTile((short)y, (short)nx, 1, pProj);
                                DrawTileAt((short)nx, (short)y, 2);
                            }
                        }
                    }
                }
            }
            else
            {
                int dy = nFireDirY;
                if (dy != 0)
                {
                    int ny = (nStep + 1) * dy + y;
                    if (ny >= 0 && w > ny)
                    {
                        if (pWeapon->frames[7] == 0x1ff)
                        {
                            short t = pWorld->currentZone->GetTile(x, ny - dy, 1);
                            if (t == -1)
                            {
                                DrawTileAt((short)x, (short)ny, 0);
                                DrawTileAt((short)x, (short)ny, 1);
                                BlitTile((short)ny, (short)x, 1, pProj);
                                DrawTileAt((short)x, (short)ny, 2);
                                pWorld->nWeaponHitX = x;
                                pWorld->nWeaponHitY = ny;
                            }
                            else
                            {
                                if ((pWorld->GetTileData(t)->flags & 0x60000) == 0)
                                {
                                    DrawTileAt((short)x, (short)ny, 0);
                                    DrawTileAt((short)x, (short)ny, 1);
                                    BlitTile((short)ny, (short)x, 1, pProj);
                                    DrawTileAt((short)x, (short)ny, 2);
                                    pWorld->nWeaponHitX = x;
                                    pWorld->nWeaponHitY = ny;
                                }
                                else
                                {
                                    nFireStep = 4;
                                }
                            }
                        }
                        else
                        {
                            short t = pWorld->currentZone->GetTile(x, ny, 1);
                            if (t != -1)
                            {
                                Tile *pT = pWorld->GetTileData(t);
                                unsigned int flags = pT->flags;
                                int usable = IsUsableTileMaybe(t);
                                if (flags & 0x60000)
                                    goto cleanup;
                                if (usable != 0)
                                {
                                    nFireStep = 4;
                                    goto cleanup;
                                }
                            }
                            else
                            {
                                t = pWorld->currentZone->GetTile(x, ny - nFireDirY, 1);
                                if (t != -1 && IsUsableTileMaybe(t) != 0)
                                {
                                    nFireStep = 4;
                                    goto cleanup;
                                }
                            }
                            DrawTileAt((short)x, (short)ny, 0);
                            DrawTileAt((short)x, (short)ny, 1);
                            BlitTile((short)ny, (short)x, 1, pProj);
                            DrawTileAt((short)x, (short)ny, 2);
                        }
                    }
                }
            }
        }
    }
cleanup:
    ;
    int i = 0;
    CDC *pDC = GetDC();
    CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
    DrawGameArea(pDC);
    pDC->SelectPalette(pOldPal, 0);
    ReleaseDC(pDC);
    UseWeapon(x, y, nFireDirX, nFireDirY, nStep);
#ifdef GAME_INDY
    if (pWeapon->unk48 <= 0 && pWeapon->frames[7] != 0x1fe &&
        !(pWorld->GetTileData(pWeapon->frames[7]) != NULL &&
          !(pWorld->GetTileData(pWeapon->frames[7])->flags & TILE_LIGHT_BLASTER)))
#else
    if (pWeapon->unk48 <= 0 && pWeapon->frames[7] != 0x1fe && pWeapon->frames[7] != 0x12)
#endif
    {
        pWorld->currentWeapon = 0;
        DrawWeaponBox(0);
        DrawWeaponIcon(0);
        Tile *pTile = pWorld->GetTileData(pWeapon->frames[7]);
        RemoveItem(pTile);
        int bFound = 0;
        int nInv = pWorld->inventory.GetSize();
        if (nInv > 0)
        {
            do
            {
                InvItem *pItem = (InvItem *)pWorld->inventory.GetAt(i);
                if (pItem->pTile == pTile)
                {
                    pWorld->currentWeapon = pWeapon;
                    switch (nWeaponTile)
                    {
                    case 0x1ff:
                        if (pWorld->weaponState[0] > 0)
                            pWeapon->unk48 = pWorld->weaponState[0];
                        else
                        {
                            pWorld->weaponState[0] = 15;
                            pWeapon->unk48 = pWorld->weaponState[0];
                        }
                        PlaySound(0x34);
                        break;
                    case 0x200:
                        if (pWorld->weaponState[1] > 0)
                            pWeapon->unk48 = pWorld->weaponState[1];
                        else
                        {
                            pWorld->weaponState[1] = 30;
                            pWeapon->unk48 = pWorld->weaponState[1];
                        }
                        PlaySound(0x20);
                        break;
                    case 0x201:
                        if (pWorld->weaponState[2] > 0)
                            pWeapon->unk48 = pWorld->weaponState[2];
                        else
                        {
                            pWorld->weaponState[2] = 10;
                            pWeapon->unk48 = pWorld->weaponState[2];
                        }
                        PlaySound(0x20);
                        break;
                    case 0x204:
                        if (pWorld->weaponState[3] > 0)
                            pWeapon->unk48 = pWorld->weaponState[3];
                        else
                        {
                            pWorld->weaponState[3] = 15;
                            pWeapon->unk48 = pWorld->weaponState[3];
                        }
                        break;
                    case 0x205:
                        if (pWorld->nCurrentAmmoMaybe > 0)
                            pWeapon->unk48 = pWorld->nCurrentAmmoMaybe;
                        else
                        {
                            pWorld->nCurrentAmmoMaybe = 15;
                            pWeapon->unk48 = pWorld->nCurrentAmmoMaybe;
                        }
                        break;
                    }
                    DrawWeaponBox(0);
                    DrawWeaponIcon(0);
                    bFound = 1;
                    break;
                }
                i++;
            } while (nInv > i);
        }
        if (bFound == 0)
        {
            int j = 0;
            int nChars = pWorld->characters.GetSize();
            if (nChars > 0)
            {
                do
                {
                    Character *pChar = (Character *)pWorld->characters.GetAt(j);
                    if (pChar->frames[7] == 0x12)
                    {
                        pChar->unk48 = 30;
                        pWorld->currentWeapon = pChar;
                        PlaySound(0x1f);
                        break;
                    }
                    j++;
                } while (nChars > j);
            }
            DrawWeaponBox(0);
            DrawWeaponIcon(0);
        }
    }
    bBusy = 0;
}

// FUNCTION: YODA 0x0040b160
// GameView::DrawEntities — stamp every active monster's current walk frame into
// layer 1 of the current zone and redraw its cell. Entities with charId < 0
// (empty slot) or active != 1 (dead/hidden) are skipped. GetWalkFrameTile
// refreshes pChar->currentFrame from the facing dir before the SetTile.
void CDeskcppView::DrawEntities()
{
    Zone *pZone = pWorld->currentZone;
    int nCount = pZone->entities.GetSize();
    if (nCount >= 1)
    {
        int i = 0;
        int n = nCount;
        if (n > 0)
        {
            do
            {
                MapEntity *pEnt = (MapEntity *)pZone->entities.GetAt(i);
                if (pEnt->charId >= 0 && pEnt->active == 1)
                {
                    YODA_SIC_FIX(if (pEnt->charId >= pWorld->characters.GetSize()) { BUGLOG(("sic#15 DrawEntities: charId=%d n=%d, OOB read avoided\n", (int)pEnt->charId, (int)pWorld->characters.GetSize())); i++; n--; continue; }) Character *pChar = (Character *)pWorld->characters.GetAt(pEnt->charId);
                    pChar->GetWalkFrameTile(0, 0, &pWorld->tiles);
                    int nFrame = pChar->currentFrame;
                    pZone->SetTile(pEnt->x, pEnt->y, 1, (short)nFrame);
                    DrawTileAt(pEnt->x, pEnt->y, -1);
                }
                i++;
                n--;
            } while (n != 0);
        }
    }
}

// FUNCTION: YODA 0x0040b210
// GameView::FindEntityAt — scan the current zone's entities for one at cell
// (x, y); returns its charId, or -1 if the cell is empty. Called on bumps to
// decide monster interaction.
// EFFECTIVE MATCH (35/35 insns, align=0, 6 byte diff): single 2-register cycle —
// the m_pData walker is EDX and pEnt ECX in the original, mirrored in ours.
// Decl order pZone/nCharId/n/i is load-bearing (probed all 12 permutations:
// three tie at align=0, the rest align 20-34). Removing the pEnt local (CSE-temp
// theory) is WORSE (align 42) — pEnt is a real local. Allocator tie-break; G1.
short CDeskcppView::FindEntityAt(int x, int y)
{
    Zone *pZone = pWorld->currentZone;
    short nCharId = -1;
    int n = pZone->entities.GetSize();
    int i = 0;
    if (n > 0)
    {
        do
        {
            MapEntity *pEnt = (MapEntity *)pZone->entities.GetAt(i);
            if (pEnt->x == x && pEnt->y == y)
            {
                nCharId = pEnt->charId;
                break;
            }
            i++;
        } while (i < n);
    }
    return nCharId;
}

// FUNCTION: YODA 0x0040b270
// GameView::Tick — the per-entity update / enemy-AI step (10.8KB), called ~5x
// per frame by OnTimer/UpdateFrame. Walks pWorld->currentZone->entities; for
// each live MapEntity looks up its Character def and (a) if it has a weapon
// char, aims/steps a projectile toward the player (bullet* fields, damage via
// AddHealth on impact), then (b) picks a movement step via the enemy-AI switch
// on Character.moveType (1 wander/melee+retreat, 2 erratic wander, 3
// wander-or-home, 4 direct chase, 6 mod-3 pattern chase/flee, 7/8/9 wander
// variants, 10 waypoint patrol, 11 flee-with-retry, 12 sit-and-animate), tests
// it with Zone::IactProbeMove, melee-attacks when the step reaches the player
// (PlaySound(4) + AddHealth(-damage)), moves if the target cell is empty and
// its floor tile lacks TILE flag 0x10000, and finally re-stamps the walk frame
// into layer 1 + DrawZoneCell. The player cell is the camera cell
// (cameraX/32, cameraY/32).
// sic: a moveType outside 1..12 (and 5) reaches the shared tail with nDX/nDY
// UNINITIALIZED from the previous entity (engine-bugs.md family); the
// pChar == NULL tail check is dead (the loop returned earlier).
//
// EFFECTIVE-WIP (v20: 2504/2336 insns — ~154 of ours are the 12 switch jump
// tables at the COMDAT end disassembling as zeros, so real code is ~2350 —
// align 2202 from ~3588 at first compile). STRUCTURE PROVEN; the residual is
// dominated by TWO GLOBAL FAMILIES plus one parked block:
// (1) cmp-DIRECTION mirror: EVERY entity-vs-player compare emits
//     cmp [slot],reg + inverted jcc in ours vs cmp reg,[slot] in the orig
//     (~40 sites, + it forces RE-CMP in flag-reuse sign chains where the orig
//     reuses flags). Identical source operand order compiles both ways —
//     lesson #6 family, correlates with the frame-slot layout being +4-shifted
//     (our pEnt slot 0x1c vs orig 0x18; px/py 0x20/0x24 vs 0x1c/0x20). One
//     global tie-break, NOT per-site steerable — G1.
// (2) register-ROLE rotation in the bullet/erase blocks and the tail
//     (pBY/pBX/pBDY/pBDX = EBX/EDI/EBP/ESI in orig, rotated in ours;
//     pChar slot-homed 0x30 in orig, reg-homed here).
// (3) PARKED: the FIRE/SHOOT block (bAimed=1 + GetProjectileTile + goto STEP,
//     ~17 insns) sits INLINE at the first rand-site in the orig (0x1fa, jne
//     over it) but cl defers it to after the horizontal arm in all SEVEN
//     source shapes probed (nested-if, if-goto both senses, fall-through with
//     explicit gotos, full per-arm duplication — the copies do NOT cross-jump).
//     Rule learned: cl 4.2 defers any block ENDING in an unconditional
//     transfer, preferring fall-through continuity (same mechanism sank
//     WorldDoc GetLocatorIcon's early-return bodies). The orig's inline copy
//     must come from a construct not yet found — decomp.me/G1.
// Layout mechanisms that DID land (reusable): the dead-entity cleanup is the
// deferred fall-through of `if (*pActive != 0) goto ALIVE_ENT;` (cl inlines
// the not-yet-emitted goto target and defers the source fall-through); case
// 11's random-step block is the labeled THEN-arm of an `||` if/else (falls
// through into the probe loop — arms that FALL THROUGH stay inline); the
// case-1 walkable zeros stay inline (fall to the retreat counter) while cases
// 2/3/6/7/8/9/10's zeros+break cross-jump into ONE shared block (case 12's
// body); the probe-result switches keep their comparison TREE only when
// case -1/0 and default have SEPARATE (duplicated) bodies.
void CDeskcppView::Tick()
{
    Zone *pZone = pWorld->currentZone;
    frameCounter++;
    int nCount = pZone->entities.GetSize();
    if (nCount >= 1)
    {
        int nPlayerX = pWorld->cameraX / 32;
        int bTurned = 0;
        int i = 0;
        int nPlayerY = pWorld->cameraY / 32;
        bBusy = 1;
        if (nCount > 0)
        {
            do
            {
                MapEntity *pEnt = (MapEntity *)pZone->entities.GetAt(i);
                short nCharId = pEnt->charId;
                int *pActive;
                YODA_SIC_FIX(if (nCharId >= pWorld->characters.GetSize()) { BUGLOG(("sic#15 Tick: charId=%d n=%d, OOB entity skipped\n", (int)nCharId, (int)pWorld->characters.GetSize())); goto NEXT_ENT; }) if (nCharId < 0)
                    goto NEXT_ENT;
                pActive = &pEnt->active;
                if (*pActive != 0)
                    goto ALIVE_ENT;
                if (nCharId >= 0)
                {
                Character *pC = (Character *)pWorld->characters.GetAt(nCharId);
                if (pC->weaponCharId >= 0)
                {
                    short *pBDY = &pEnt->bulletDY;
                    short *pBDX = &pEnt->bulletDX;
                    YODA_SIC_FIX(if (pC->weaponCharId >= pWorld->characters.GetSize()) { BUGLOG(("sic#15 Tick: weaponCharId=%d n=%d, OOB entity skipped\n", (int)pC->weaponCharId, (int)pWorld->characters.GetSize())); goto NEXT_ENT; }) Tile *pProj = (Tile *)((Character *)pWorld->characters.GetAt(pC->weaponCharId))->GetProjectileTile(0, *pBDX, *pBDY, 0, &pWorld->tiles);
                    int t = pWorld->FindTile(pProj);
                    short *pBY = &pEnt->bulletY;
                    short *pBX = &pEnt->bulletX;
                    if ((short)pZone->GetTile(*pBX, *pBY, 1) == t
                        && (*pBX != nPlayerX || *pBY != nPlayerY || pEnt->active != 0))
                    {
                        pZone->SetTile(*pBX, *pBY, 1, -1);
                        DrawZoneCell(*pBX, *pBY);
                    }
                    pEnt->bulletStep = 0;
                    *pBX = pEnt->x;
                    *pBY = pEnt->y;
                    *pBDX = 0;
                    *pBDY = 0;
                }
                }
                goto NEXT_ENT;
            ALIVE_ENT:
                {
                Character *pChar = (Character *)pWorld->characters.GetAt(nCharId);
                Character *pWeapon;
                int nDX, nDY;
                int nRet;
                Tile *pProjTile;
                if (pChar->weaponCharId < 0)
                    pWeapon = NULL;
                else
                    pWeapon = (Character *)pWorld->characters.GetAt(pChar->weaponCharId);
                if (pChar == NULL)
                {
                    bBusy = 0;
                    return;
                }
                int bMoved = 0;
                int bAimed = 0;
                if (pWeapon != NULL)
                {
                    short *pBDY;
                    short *pBDX;
                    short *pBulletStep = &pEnt->bulletStep;
                    if (*pBulletStep == 0 && pEnt->timer <= 0)
                    {
                        short *pX = &pEnt->x;
                        short sx = *pX;
                        if (abs(sx - nPlayerX) < 4)
                        {
                            short *pY = &pEnt->y;
                            if (abs(*pY - nPlayerY) < 4 && *pActive != 0)
                            {
                                pEnt->bulletX = sx;
                                pEnt->bulletY = *pY;
                                if (*pX == nPlayerX)
                                {
                                    if (*pY > nPlayerY)
                                        pEnt->bulletDY = -1;
                                    else if (*pY < nPlayerY)
                                        pEnt->bulletDY = 1;
                                    pEnt->bulletDX = 0;
                                    if (pChar->moveType == 4)
                                    {
                                        if (rand() % 7 != 3)
                                            goto NOSHOT_V;
                                    FIRE:
                                        bAimed = 1;
                                    SHOOT:
                                        pBDY = &pEnt->bulletDY;
                                        pBDX = &pEnt->bulletDX;
                                        pProjTile = (Tile *)pWeapon->GetProjectileTile(0, *pBDX, *pBDY, 0, &pWorld->tiles);
                                        goto BULLET_STEP;
                                    NOSHOT_V:
                                        pEnt->bulletDY = 0;
                                    }
                                    else
                                    {
                                        if (rand() % 7 == 3)
                                            goto FIRE;
                                        pEnt->bulletDY = 0;
                                    }
                                }
                                else
                                {
                                    if (*pY != nPlayerY)
                                        goto SHOOT;
                                    if (*pX > nPlayerX)
                                        pEnt->bulletDX = -1;
                                    else if (*pX < nPlayerX)
                                        pEnt->bulletDX = 1;
                                    pEnt->bulletDY = 0;
                                    if (pChar->moveType == 4)
                                    {
                                        if (rand() % 7 == 3)
                                            goto FIRE;
                                        pEnt->bulletDX = 0;
                                    }
                                    else
                                    {
                                        if (rand() % 7 == 3)
                                            goto FIRE;
                                        pEnt->bulletDX = 0;
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        pBDY = &pEnt->bulletDY;
                        pBDX = &pEnt->bulletDX;
                        pProjTile = (Tile *)pWeapon->GetProjectileTile(0, *pBDX, *pBDY, 0, &pWorld->tiles);
                        bAimed = 0;
                    BULLET_STEP:
                        {
                            (*pBulletStep)++;
                            short *pBY = &pEnt->bulletY;
                            short *pBX = &pEnt->bulletX;
                            nRet = pZone->IactProbeMove(*pBX, *pBY, *pBDX, *pBDY, (PTRINT)&pWorld->tiles, 0);
                            if (nRet != 1)
                            {
                                bMoved = 0;
                                *pBDX = 0;
                                *pBDY = 0;
                            }
                            else
                            {
                                short sdx = *pBDX;
                                short sbx = *pBX;
                                if (sdx + sbx == nPlayerX && *pBY + *pBDY == nPlayerY)
                                {
                                    *pBDX = 0;
                                    *pBDY = 0;
                                    AddHealth(-pChar->damage);
                                    bMoved = 0;
                                    PlaySound(4);
                                }
                                else
                                {
                                    bMoved = 1;
                                    *pBX = sbx + sdx;
                                    *pBY = *pBY + *pBDY;
                                    if (*pBulletStep == 1 && bAimed)
                                        PlaySoundData(pWeapon->weaponCharId);
                                }
                            }
                            if (bMoved && *pBulletStep < 4)
                            {
                                pZone->SetTile(*pBX - *pBDX, *pBY - *pBDY, 1, -1);
                                DrawZoneCell(*pBX - *pBDX, *pBY - *pBDY);
                                int t = pWorld->FindTile(pProjTile);
                                pZone->SetTile(*pBX, *pBY, 1, (short)t);
                                DrawZoneCell(*pBX, *pBY);
                            }
                            else
                            {
                                int t = pWorld->FindTile(pProjTile);
                                if ((short)pZone->GetTile(*pBX - *pBDX, *pBY - *pBDY, 1) == t)
                                {
                                    pZone->SetTile(*pBX - *pBDX, *pBY - *pBDY, 1, -1);
                                    DrawZoneCell(*pBX - *pBDX, *pBY - *pBDY);
                                }
                                *pBulletStep = 0;
                                *pBX = pEnt->x;
                                *pBY = pEnt->y;
                                *pBDX = 0;
                                *pBDY = 0;
                            }
                        }
                    }
                }
                if (*pActive != 0)
                {
                    switch (pChar->moveType)
                    {
                    case 1:
                        if (pEnt->timer != 0)
                        {
                            nDY = 0;
                            nDX = 0;
                            pEnt->timer--;
                        }
                        else if (rand() % 2 == 0)
                        {
                            nDY = 0;
                            nDX = 0;
                        }
                        else
                        {
                            int *pRetreat = &pEnt->bRetreating;
                            if (*pRetreat != 0)
                            {
                                if (rand() % 2 == 0)
                                {
                                    if (pEnt->x > nPlayerX)
                                        nDX = 1;
                                    else
                                    {
                                        nDX = -1;
                                        if (pEnt->x >= nPlayerX)
                                            nDX = 0;
                                    }
                                    if (pEnt->y > nPlayerY)
                                        nDY = 1;
                                    else if (pEnt->y < nPlayerY)
                                        nDY = -1;
                                    else
                                        nDY = 0;
                                }
                                else
                                {
                                    nDX = rand() % 3 - 1;
                                    nDY = rand() % 3 - 1;
                                }
                                short nStep = pEnt->aiStepCounter + 1;
                                pEnt->aiStepCounter = nStep;
                                if (nStep > 3)
                                {
                                    *pRetreat = 0;
                                    pEnt->aiStepCounter = 0;
                                }
                            }
                            else
                            {
                                if (pEnt->x > nPlayerX)
                                    nDX = -1;
                                else
                                {
                                    nDX = 1;
                                    if (pEnt->x >= nPlayerX)
                                        nDX = 0;
                                }
                                if (pEnt->y > nPlayerY)
                                    nDY = -1;
                                else
                                {
                                    nDY = 1;
                                    if (pEnt->y >= nPlayerY)
                                        nDY = 0;
                                }
                            }
                            short *pY = &pEnt->y;
                            short *pX = &pEnt->x;
                            nRet = pZone->IactProbeMove(*pX, *pY, nDX, nDY, (PTRINT)&pWorld->tiles, 0);
                            switch (nRet)
                            {
                            case 2: nDX++; break;
                            case 3: nDX--; break;
                            case 4: nDY++; break;
                            case 5: nDY--; break;
                            case -1:
                            case 0:
                                nDY = 0;
                                nDX = 0;
                                break;
                            }
                            if (*pX + nDX == nPlayerX && *pY + nDY == nPlayerY)
                            {
                                nDX = 0;
                                nDY = 0;
                                if (pChar->damage >= 0)
                                {
                                    PlaySound(4);
                                    AddHealth(-pChar->damage);
                                }
                                short nStep = pEnt->aiStepCounter + 1;
                                pEnt->aiStepCounter = nStep;
                                if (nStep > 1)
                                {
                                    *pRetreat = 1;
                                    pEnt->aiStepCounter = 0;
                                }
                            }
                            short t = pZone->GetTile(*pX + nDX, *pY + nDY, 1);
                            if (t != -1)
                            {
                                nDY = 0;
                                nDX = 0;
                            }
                            else
                            {
                                bMoved = 0;
                                t = pZone->GetTile(*pX + nDX, *pY + nDY, 0);
                                if (t >= 0 && (pWorld->GetTileData(t)->flags & 0x10000) == 0)
                                    bMoved = 1;
                                if (bMoved)
                                {
                                    *pX += (short)nDX;
                                    *pY += (short)nDY;
                                }
                            }
                            if (*pRetreat == 0)
                            {
                                short nStep = pEnt->aiStepCounter + 1;
                                pEnt->aiStepCounter = nStep;
                                if (nStep >= 14)
                                {
                                    *pRetreat = 1;
                                    pEnt->aiStepCounter = 0;
                                }
                            }
                        }
                        break;
                    case 2:
                        if (pEnt->timer != 0)
                        {
                            nDY = 0;
                            nDX = 0;
                            pEnt->timer--;
                            break;
                        }
                        if (rand() % 2 == 0)
                        {
                            nDX = 0;
                            nDY = 0;
                            break;
                        }
                        {
                            short *pX = &pEnt->x;
                            if (abs(*pX - nPlayerX) < 2 && abs(pEnt->y - nPlayerY) < 2)
                            {
                                if (rand() % 2 == 0)
                                {
                                    nDX = rand() % 3 - 1;
                                    nDY = rand() % 3 - 1;
                                }
                                else
                                {
                                    bTurned = 1;
                                    if (*pX > nPlayerX)
                                        nDX = -1;
                                    else
                                    {
                                        nDX = 1;
                                        if (*pX >= nPlayerX)
                                            nDX = 0;
                                    }
                                    if (pEnt->y > nPlayerY)
                                        nDY = -1;
                                    else if (pEnt->y < nPlayerY)
                                        nDY = 1;
                                    else
                                        nDY = 0;
                                }
                            }
                            else
                            {
                                bTurned = 0;
                                switch (pEnt->wanderDir)
                                {
                                case 0: nDX = 0; nDY = 1; break;
                                case 1: nDX = 1; nDY = 0; break;
                                case 2: nDX = -1; nDY = 0; break;
                                case -1: nDX = 0; nDY = -1; break;
                                }
                            }
                            short *pY = &pEnt->y;
                            nRet = pZone->IactProbeMove(*pX, *pY, nDX, nDY, (PTRINT)&pWorld->tiles, 0);
                            if (bTurned)
                            {
                                switch (nRet)
                                {
                                case 2: nDX++; break;
                                case 3: nDX--; break;
                                case 4: nDY++; break;
                                case 5: nDY--; break;
                                case -1:
                                case 0:
                                    nDY = 0;
                                    nDX = 0;
                                    break;
                                }
                            }
                            else
                            {
                                switch (nRet)
                                {
                                case 1:
                                    break;
                                case -1:
                                case 0:
                                    nDY = 0;
                                    nDX = 0;
                                    pEnt->timer = (short)(rand() % 3);
                                    pEnt->wanderDir = (short)(rand() % 4) - 1;
                                    break;
                                default:
                                    nDY = 0;
                                    nDX = 0;
                                    pEnt->timer = (short)(rand() % 3);
                                    pEnt->wanderDir = (short)(rand() % 4) - 1;
                                    break;
                                }
                            }
                            if (*pX + nDX == nPlayerX && *pY + nDY == nPlayerY)
                            {
                                nDX = 0;
                                nDY = 0;
                                if (pChar->damage >= 0)
                                {
                                    PlaySound(4);
                                    AddHealth(-pChar->damage);
                                }
                            }
                            short t = pZone->GetTile(*pX + nDX, *pY + nDY, 1);
                            if (t != -1)
                            {
                                nDX = 0;
                                nDY = 0;
                                break;
                            }
                                bMoved = 0;
                                t = pZone->GetTile(*pX + nDX, *pY + nDY, 0);
                                if (t >= 0 && (pWorld->GetTileData(t)->flags & 0x10000) == 0)
                                    bMoved = 1;
                                if (bMoved)
                                {
                                    *pX += (short)nDX;
                                    *pY += (short)nDY;
                                }
                        }
                        break;
                    case 3:
                        if (pEnt->timer != 0)
                        {
                            nDY = 0;
                            nDX = 0;
                            pEnt->timer--;
                            break;
                        }
                        if (rand() % 2 == 0)
                        {
                            nDX = 0;
                            nDY = 0;
                            break;
                        }
                        {
                            short *pX = &pEnt->x;
                            if (abs(*pX - nPlayerX) < 2 && abs(pEnt->y - nPlayerY) < 2)
                            {
                                if (rand() % 2 == 0)
                                    goto RANDOM3;
                                if (*pX > nPlayerX)
                                    nDX = -1;
                                else
                                {
                                    nDX = 1;
                                    if (*pX >= nPlayerX)
                                        nDX = 0;
                                }
                                if (pEnt->y > nPlayerY)
                                    nDY = -1;
                                else if (pEnt->y < nPlayerY)
                                    nDY = 1;
                                else
                                    nDY = 0;
                            }
                            else
                            {
                            RANDOM3:
                                nDX = rand() % 3 - 1;
                                nDY = rand() % 3 - 1;
                            }
                            short *pY = &pEnt->y;
                            nRet = pZone->IactProbeMove(*pX, *pY, nDX, nDY, (PTRINT)&pWorld->tiles, 0);
                            switch (nRet)
                            {
                            case 2: nDX++; break;
                            case 3: nDX--; break;
                            case 4: nDY++; break;
                            case 5: nDY--; break;
                            case -1:
                            case 0:
                                nDY = 0;
                                nDX = 0;
                                break;
                            }
                            if (*pX + nDX == nPlayerX && *pY + nDY == nPlayerY)
                            {
                                nDX = 0;
                                nDY = 0;
                                if (pWeapon == NULL && pChar->damage >= 0)
                                {
                                    PlaySound(4);
                                    AddHealth(-pChar->damage);
                                }
                            }
                            short t = pZone->GetTile(*pX + nDX, *pY + nDY, 1);
                            if (t != -1)
                            {
                                nDX = 0;
                                nDY = 0;
                                break;
                            }
                                bMoved = 0;
                                t = pZone->GetTile(*pX + nDX, *pY + nDY, 0);
                                if (t >= 0 && (pWorld->GetTileData(t)->flags & 0x10000) == 0)
                                    bMoved = 1;
                                if (bMoved)
                                {
                                    *pX += (short)nDX;
                                    *pY += (short)nDY;
                                }
                        }
                        break;
                    case 4:
                        if (pEnt->x > nPlayerX)
                            nDX = -1;
                        else
                        {
                            nDX = 1;
                            if (pEnt->x >= nPlayerX)
                                nDX = 0;
                        }
                        if (pEnt->y > nPlayerY)
                            nDY = -1;
                        else
                        {
                            nDY = 1;
                            if (pEnt->y >= nPlayerY)
                                nDY = 0;
                        }
                        bMoved = 0;
                        break;
                    case 6:
                        if (pEnt->timer != 0)
                        {
                            nDY = 0;
                            nDX = 0;
                            pEnt->timer--;
                            break;
                        }
                        {
                            short nFrame = frameCounter;
                            if (nFrame % 3 != 0)
                            {
                                nDX = 0;
                                nDY = 0;
                                break;
                            }
                            short *pMode = &pEnt->unk10;
                            if (*pMode == 0)
                            {
                                *pMode = 1;
                                if (nFrame % 10 != 0)
                                    *pMode = 0;
                                if (*pMode != 0)
                                    goto CHASE6;
                                if (pEnt->x < nPlayerX)
                                    nDX = -1;
                                else
                                {
                                    nDX = 1;
                                    if (pEnt->x <= nPlayerX)
                                        nDX = 0;
                                }
                                if (pEnt->y < nPlayerY)
                                    nDY = -1;
                                else
                                {
                                    nDY = 1;
                                    if (pEnt->y <= nPlayerY)
                                        nDY = 0;
                                }
                            }
                            else
                            {
                            CHASE6:
                                if (pEnt->x > nPlayerX)
                                    nDX = -1;
                                else
                                {
                                    nDX = 1;
                                    if (pEnt->x >= nPlayerX)
                                        nDX = 0;
                                }
                                if (pEnt->y > nPlayerY)
                                    nDY = -1;
                                else if (pEnt->y < nPlayerY)
                                    nDY = 1;
                                else
                                    nDY = 0;
                            }
                            short *pX = &pEnt->x;
                            short *pY = &pEnt->y;
                            if (abs(*pX - nPlayerX) < 6 && abs(*pY - nPlayerY) < 6)
                            {
                                nDX = rand() % 3 - 1;
                                nDY = rand() % 3 - 1;
                            }
                            nRet = pZone->IactProbeMove(*pX, *pY, nDX, nDY, (PTRINT)&pWorld->tiles, 0);
                            switch (nRet)
                            {
                            case 2: nDX++; break;
                            case 3: nDX--; break;
                            case 4: nDY++; break;
                            case 5: nDY--; break;
                            case -1:
                            case 0:
                                nDY = 0;
                                nDX = 0;
                                break;
                            }
                            if (*pX + nDX == nPlayerX && *pY + nDY == nPlayerY)
                            {
                                nDX = 0;
                                nDY = 0;
                                if (pWeapon == NULL && pChar->damage >= 0)
                                {
                                    if (frameCounter % 3 == 0)
                                        PlaySound(4);
                                    AddHealth(-pChar->damage);
                                }
                                short nStep = *pMode + 1;
                                *pMode = nStep;
                                if (nStep > 4)
                                    *pMode = 0;
                            }
                            short t = pZone->GetTile(*pX + nDX, *pY + nDY, 1);
                            if (t != -1)
                            {
                                nDX = 0;
                                nDY = 0;
                                break;
                            }
                                bMoved = 0;
                                t = pZone->GetTile(*pX + nDX, *pY + nDY, 0);
                                if (t >= 0 && (pWorld->GetTileData(t)->flags & 0x10000) == 0)
                                    bMoved = 1;
                                if (bMoved)
                                {
                                    *pX += (short)nDX;
                                    *pY += (short)nDY;
                                }
                        }
                        break;
                    case 7:
                        if (pEnt->timer != 0)
                        {
                            nDY = 0;
                            nDX = 0;
                            pEnt->timer--;
                            break;
                        }
                        if (rand() % 2 == 0)
                        {
                            nDX = 0;
                            nDY = 0;
                            break;
                        }
                        {
                            int nX = pEnt->x;
                            short *pX = &pEnt->x;
                            if (abs(nX - nPlayerX) < 2 && abs(pEnt->y - nPlayerY) < 2)
                            {
                                bTurned = 1;
                                if (nX > nPlayerX)
                                    nDX = -1;
                                else
                                {
                                    nDX = 1;
                                    if (nX >= nPlayerX)
                                        nDX = 0;
                                }
                                if (pEnt->y > nPlayerY)
                                    nDY = -1;
                                else if (pEnt->y < nPlayerY)
                                    nDY = 1;
                                else
                                    nDY = 0;
                            }
                            else
                            {
                                bTurned = 0;
                                switch (pEnt->wanderDir)
                                {
                                case 0: nDX = 0; nDY = 1; break;
                                case 1: nDX = 1; nDY = 0; break;
                                case 2: nDX = -1; nDY = 0; break;
                                case -1: nDX = 0; nDY = -1; break;
                                }
                            }
                            short *pY = &pEnt->y;
                            nRet = pZone->IactProbeMove(nX, *pY, nDX, nDY, (PTRINT)&pWorld->tiles, 0);
                            if (bTurned)
                            {
                                switch (nRet)
                                {
                                case 2: nDX++; break;
                                case 3: nDX--; break;
                                case 4: nDY++; break;
                                case 5: nDY--; break;
                                case -1:
                                case 0:
                                    nDY = 0;
                                    nDX = 0;
                                    break;
                                }
                            }
                            else
                            {
                                switch (nRet)
                                {
                                case 1:
                                    break;
                                case -1:
                                case 0:
                                    nDY = 0;
                                    nDX = 0;
                                    pEnt->timer = (short)(rand() % 8);
                                    pEnt->wanderDir = (short)(rand() % 4) - 1;
                                    break;
                                default:
                                    nDY = 0;
                                    nDX = 0;
                                    pEnt->timer = (short)(rand() % 8);
                                    pEnt->wanderDir = (short)(rand() % 4) - 1;
                                    break;
                                }
                            }
                            if (*pX + nDX == nPlayerX && *pY + nDY == nPlayerY)
                            {
                                nDX = 0;
                                nDY = 0;
                                if (pChar->damage >= 0)
                                {
                                    PlaySound(4);
                                    AddHealth(-pChar->damage);
                                }
                            }
                            short t = pZone->GetTile(*pX + nDX, *pY + nDY, 1);
                            if (t != -1)
                            {
                                nDX = 0;
                                nDY = 0;
                                break;
                            }
                                bMoved = 0;
                                t = pZone->GetTile(*pX + nDX, *pY + nDY, 0);
                                if (t >= 0 && (pWorld->GetTileData(t)->flags & 0x10000) == 0)
                                    bMoved = 1;
                                if (bMoved)
                                {
                                    *pX += (short)nDX;
                                    *pY += (short)nDY;
                                }
                        }
                        break;
                    case 8:
                        if (pEnt->timer != 0)
                        {
                            nDY = 0;
                            nDX = 0;
                            pEnt->timer--;
                            break;
                        }
                        {
                            short *pX = &pEnt->x;
                            if (abs(*pX - nPlayerX) < 2 && abs(pEnt->y - nPlayerY) < 2)
                            {
                                bTurned = 1;
                                if (rand() % 2 == 0)
                                {
                                    nDX = rand() % 3 - 1;
                                    nDY = rand() % 3 - 1;
                                }
                                else
                                {
                                    if (*pX > nPlayerX)
                                        nDX = -1;
                                    else
                                    {
                                        nDX = 1;
                                        if (*pX >= nPlayerX)
                                            nDX = 0;
                                    }
                                    if (pEnt->y > nPlayerY)
                                        nDY = -1;
                                    else if (pEnt->y < nPlayerY)
                                        nDY = 1;
                                    else
                                        nDY = 0;
                                }
                            }
                            else if (rand() % 3 == 0)
                            {
                                nDX = rand() % 3 - 1;
                                bTurned = 1;
                                nDY = rand() % 3 - 1;
                            }
                            else
                            {
                                bTurned = 0;
                                switch (pEnt->wanderDir)
                                {
                                case 0: nDX = 0; nDY = 1; break;
                                case 1: nDX = 1; nDY = 0; break;
                                case 2: nDX = -1; nDY = 0; break;
                                case -1: nDX = 0; nDY = -1; break;
                                }
                            }
                            short *pY = &pEnt->y;
                            nRet = pZone->IactProbeMove(*pX, *pY, nDX, nDY, (PTRINT)&pWorld->tiles, 0);
                            if (bTurned)
                            {
                                switch (nRet)
                                {
                                case 2: nDX++; break;
                                case 3: nDX--; break;
                                case 4: nDY++; break;
                                case 5: nDY--; break;
                                case -1:
                                case 0:
                                    nDY = 0;
                                    nDX = 0;
                                    break;
                                }
                            }
                            else
                            {
                                switch (nRet)
                                {
                                case 1:
                                    break;
                                case -1:
                                case 0:
                                    nDY = 0;
                                    nDX = 0;
                                    pEnt->timer = (short)(rand() % 10);
                                    pEnt->wanderDir = (short)(rand() % 4) - 1;
                                    break;
                                default:
                                    nDY = 0;
                                    nDX = 0;
                                    pEnt->timer = (short)(rand() % 10);
                                    pEnt->wanderDir = (short)(rand() % 4) - 1;
                                    break;
                                }
                            }
                            if (*pX + nDX == nPlayerX && *pY + nDY == nPlayerY)
                            {
                                nDX = 0;
                                nDY = 0;
                                if (pWeapon == NULL && pChar->damage >= 0)
                                {
                                    if (rand() % 2 == 0)
                                    {
                                        PlaySound(4);
                                        AddHealth(-pChar->damage);
                                    }
                                }
                            }
                            short t = pZone->GetTile(*pX + nDX, *pY + nDY, 1);
                            if (t != -1)
                            {
                                nDX = 0;
                                nDY = 0;
                                break;
                            }
                                bMoved = 0;
                                t = pZone->GetTile(*pX + nDX, *pY + nDY, 0);
                                if (t >= 0 && (pWorld->GetTileData(t)->flags & 0x10000) == 0)
                                    bMoved = 1;
                                if (bMoved)
                                {
                                    *pX += (short)nDX;
                                    *pY += (short)nDY;
                                }
                        }
                        break;
                    case 9:
                        if (pEnt->timer != 0)
                        {
                            nDY = 0;
                            nDX = 0;
                            pEnt->timer--;
                            break;
                        }
                        if (frameCounter % 2 == 0)
                        {
                            nDX = 0;
                            nDY = 0;
                            break;
                        }
                        {
                            switch (pEnt->wanderDir)
                            {
                            case 0: nDX = 0; nDY = 1; break;
                            case 1: nDX = 1; nDY = 0; break;
                            case 2: nDX = -1; nDY = 0; break;
                            case -1: nDX = 0; nDY = -1; break;
                            }
                            short *pY = &pEnt->y;
                            short *pX = &pEnt->x;
                            nRet = pZone->IactProbeMove(*pX, *pY, nDX, nDY, (PTRINT)&pWorld->tiles, 0);
                            switch (nRet)
                            {
                            case 1:
                                break;
                            case -1:
                            case 0:
                                nDY = 0;
                                nDX = 0;
                                pEnt->timer = (short)(rand() % 3);
                                pEnt->wanderDir = (short)(rand() % 4) - 1;
                                break;
                            default:
                                nDY = 0;
                                nDX = 0;
                                pEnt->timer = (short)(rand() % 3);
                                pEnt->wanderDir = (short)(rand() % 4) - 1;
                                break;
                            }
                            if (*pX + nDX == nPlayerX && *pY + nDY == nPlayerY)
                            {
                                nDX = 0;
                                nDY = 0;
                                if (pWeapon == NULL && pChar->damage >= 0)
                                {
                                    PlaySound(4);
                                    AddHealth(-pChar->damage);
                                }
                            }
                            short t = pZone->GetTile(*pX + nDX, *pY + nDY, 1);
                            if (t != -1)
                            {
                                nDX = 0;
                                nDY = 0;
                                break;
                            }
                                bMoved = 0;
                                t = pZone->GetTile(*pX + nDX, *pY + nDY, 0);
                                if (t >= 0 && (pWorld->GetTileData(t)->flags & 0x10000) == 0)
                                    bMoved = 1;
                                if (bMoved)
                                {
                                    *pX += (short)nDX;
                                    *pY += (short)nDY;
                                }
                        }
                        break;
                    case 10:
                        {
                            int nTX = pEnt->waypoints[pEnt->seqIdx * 2 + 2];
                            int nTY = pEnt->waypoints[pEnt->seqIdx * 2 + 3];
                            if (pEnt->timer != 0)
                            {
                                nDY = 0;
                                nDX = 0;
                                pEnt->timer--;
                                break;
                            }
                            if (frameCounter % 2 == 0)
                            {
                                nDX = 0;
                                nDY = 0;
                                break;
                            }
                            short *pX = &pEnt->x;
                            if (nTX == pEnt->x)
                                nDX = 0;
                            else if (pEnt->x <= nTX)
                                nDX = 1;
                            else
                                nDX = -1;
                            short *pY = &pEnt->y;
                            if (nTY == pEnt->y)
                                nDY = 0;
                            else if (pEnt->y <= nTY)
                                nDY = 1;
                            else
                                nDY = -1;
                            if (nDX == 0 && nDY == 0)
                            {
                                short nStep = pEnt->seqIdx + 1;
                                pEnt->seqIdx = nStep;
                                if (nStep > 2)
                                    pEnt->seqIdx = -1;
                            }
                            if (*pX + nDX == nPlayerX && *pY + nDY == nPlayerY)
                            {
                                nDX = 0;
                                nDY = 0;
                                if (pChar->damage >= 0)
                                {
                                    PlaySound(4);
                                    AddHealth(-pChar->damage);
                                }
                            }
                            short t = pZone->GetTile(*pX + nDX, *pY + nDY, 1);
                            if (t != -1)
                            {
                                nDX = 0;
                                nDY = 0;
                                break;
                            }
                                bMoved = 0;
                                t = pZone->GetTile(*pX + nDX, *pY + nDY, 0);
                                if (t >= 0 && (pWorld->GetTileData(t)->flags & 0x10000) == 0)
                                    bMoved = 1;
                                if (bMoved)
                                {
                                    *pX += (short)nDX;
                                    *pY += (short)nDY;
                                }
                        }
                        break;
                    case 11:
                        {
                            short *pY;
                            short *pX;
                            short t;
                            if (abs(pEnt->x - nPlayerX) >= 6 || abs(pEnt->y - nPlayerY) >= 6)
                            {
                            RANDOM11:
                                nDX = rand() % 3 - 1;
                                nDY = rand() % 3 - 1;
                            }
                            else
                            {
                                if (pEnt->x > nPlayerX)
                                    nDX = 1;
                                else
                                {
                                    nDX = -1;
                                    if (pEnt->x >= nPlayerX)
                                        nDX = 0;
                                }
                                if (pEnt->y > nPlayerY)
                                    nDY = 1;
                                else if (pEnt->y < nPlayerY)
                                    nDY = -1;
                                else
                                    nDY = 0;
                            }
                            {
                                pY = &pEnt->y;
                                pX = &pEnt->x;
                                nRet = pZone->IactProbeMove(*pX, *pY, nDX, nDY, (PTRINT)&pWorld->tiles, 0);
                                switch (nRet)
                                {
                                case 2: nDX++; break;
                                case 3: nDX--; break;
                                case 4: nDY++; break;
                                case 5: nDY--; break;
                                case -1:
                                case 0:
                                    nDY = 0;
                                    nDX = 0;
                                    break;
                                }
                                if (*pX + nDX == nPlayerX && *pY + nDY == nPlayerY)
                                {
                                    nDX = 0;
                                    nDY = 0;
                                    if (pChar->damage >= 0)
                                    {
                                        PlaySound(4);
                                        AddHealth(-pChar->damage);
                                    }
                                }
                                t = pZone->GetTile(*pX + nDX, *pY + nDY, 1);
                                if (t != -1)
                                {
                                    nDX = 0;
                                    nDY = 0;
                                    goto TICK_TAIL;
                                }
                                bMoved = 0;
                                t = pZone->GetTile(*pX + nDX, *pY + nDY, 0);
                                if (t < 0)
                                    goto MOVE11;
                                if (pWorld->GetTileData(t)->flags & 0x10000)
                                    goto RANDOM11;
                            }
                            bMoved = 1;
                        MOVE11:
                            if (bMoved)
                            {
                                pEnt->x += (short)nDX;
                                pEnt->y += (short)nDY;
                            }
                        }
                        break;
                    case 12:
                        nDX = 0;
                        nDY = 0;
                        break;
                    }
                TICK_TAIL:
                    if (pChar != NULL)
                    {
                        if (pChar->moveType == 3)
                        {
                            if (nDX == 0 && nDY == 0)
                            {
                                if (pEnt->x < nPlayerX)
                                    nDX = 1;
                                else
                                {
                                    nDX = -1;
                                    if (pEnt->x <= nPlayerX)
                                        nDX = 0;
                                }
                                if (pEnt->y < nPlayerY)
                                    nDY = 1;
                                else if (pEnt->y > nPlayerY)
                                    nDY = -1;
                                else
                                    nDY = 0;
                            }
                            else if (bMoved)
                            {
                                pZone->SetTile(pEnt->x - nDX, pEnt->y - nDY, 1, -1);
                                DrawZoneCell(pEnt->x - nDX, pEnt->y - nDY);
                            }
                            pChar->GetWalkFrameTile(nDX, nDY, &pWorld->tiles);
                        }
                        else
                        {
                            if (nDX == 0 && nDY == 0)
                            {
                                if (pEnt->x == nPlayerX || pEnt->y == nPlayerY)
                                {
                                    if (pEnt->x < nPlayerX)
                                        nDX = 1;
                                    else
                                    {
                                        nDX = -1;
                                        if (pEnt->x <= nPlayerX)
                                            nDX = 0;
                                    }
                                    if (pEnt->y < nPlayerY)
                                        nDY = 1;
                                    else
                                    {
                                        nDY = -1;
                                        if (pEnt->y <= nPlayerY)
                                            nDY = 0;
                                    }
                                    pChar->GetWalkFrameTile(nDX, nDY, &pWorld->tiles);
                                }
                            }
                            else if (bMoved)
                            {
                                pZone->SetTile(pEnt->x - nDX, pEnt->y - nDY, 1, -1);
                                DrawZoneCell(pEnt->x - nDX, pEnt->y - nDY);
                                pChar->GetWalkFrameTile(nDX, nDY, &pWorld->tiles);
                            }
                            else if (pEnt->bRefreshFrame != 0 || pChar->moveType == 4)
                            {
                                pChar->GetWalkFrameTile(nDX, nDY, &pWorld->tiles);
                                pEnt->bRefreshFrame = 0;
                            }
                            if (pEnt->bRefreshFrame != 0)
                            {
                                pChar->GetWalkFrameTile(nDX, nDY, &pWorld->tiles);
                                pEnt->bRefreshFrame = 0;
                            }
                        }
                        pZone->SetTile(pEnt->x, pEnt->y, 1, pChar->currentFrame);
                        if (pChar->moveType == 12)
                        {
                            short f = pChar->frames[pEnt->seqIdx];
                            pChar->currentFrame = f;
                            pZone->SetTile(pEnt->x, pEnt->y, 1, f);
                            short nStep = pEnt->seqIdx + 1;
                            pEnt->seqIdx = nStep;
                            if (nStep > 5)
                                pEnt->seqIdx = 0;
                        }
                    }
                    DrawZoneCell(pEnt->x, pEnt->y);
                }
                }
            NEXT_ENT:
                i++;
            } while (nCount > i);
        }
        bBusy = 0;
    }
}

// FUNCTION: YODA 0x0040d470
// GameView::OnTimer — ON_WM_TIMER, THE per-frame pump. Gate: nIDEvent must be
// our timer (or the 0xabcd force value) and !bBusy. Difficulty smoothing
// (counter converges on difficulty by halves), win/lose handling on
// World.abortFrame (win: score + TextOut on the viewport canvas + profile
// Count/HScore/LScore; lose: LCount++, swap to the loss zone), then the
// frame-mode dispatch (see docs/game-logic.md): 1/4 idle palette, 2 play
// (Tick + the 0x21-0x28 move-command decode -> OnBumpTile), 3 dialogue/idle
// (Tick + walk frame + map + IactRun trigger 1), 5 modal wait, 6 zone
// transition per nMapChangeReason, 7 post-transition (win counter / locator
// blink), 8 weapon fire, 9 pickup blink, 0xb world entry (+ scrollbar reset,
// deferred replay OnLoadWorld), 0xc/0xd fatal, 0xe pause dither.
// sic: the lose arm calls GetProfileInt("OPTIONS","LCount") and DISCARDS the
// result before incrementing the in-memory count.
//
// EFFECTIVE-WIP (v21 first pass: 1012/989 insns — the excess is jump-table
// data at the COMDAT end — align 802, and identity_miss 425 is ONE global
// role swap: the reloaded `this` ([ebp-0x10]) lives in EDI in the orig, EDX
// here). Landed shapes: the duplicated `nTransitionStep == 0 && g_bReplayMode`
// head condition (FireWeaponStep family), `default:` BEFORE `case 8:` in the
// reason switch (the orig ladder tests 3 then 4 then jumps out, case 8 last),
// case-9 blink arms blit-first, the `> 7` scrollbar arm order, `<= 10` return
// literals. Parked residuals: the 4 per-case `nFrameMode = 3` copies cross-
// jump in ours but stay separate in the orig (same open family as Tick's fire
// block); pApp spilled to the CString-shared slot [ebp-0x14] in the orig vs
// reg here (function-scope decl probe was WORSE); IactRun arg-push scheduling;
// our 3 ::SetScrollRange calls cache the import pointer in EBX (orig calls
// through the import each time — inverse of the v19 ZTS GetDC quirk).
void CDeskcppView::OnTimer(UINT nIDEvent)
{
    if (pWorld->difficulty != pWorld->counter)
        pWorld->counter = (pWorld->difficulty + pWorld->counter) / 2;
    if ((nIDEvent != 0xabcd && nTimerId != nIDEvent) || bBusy != 0)
        return;
    if (bMouseCaptured != 0 && pWorld->nFrameMode == 3 && bMapAtCanvasOriginMaybe == 0)
        pWorld->nFrameMode = 2;
    if (pWorld->abortFrame != 0)
    {
        if (pWorld->abortFrame == 1)
        {
            pWorld->gameState = 1;
            pWorld->pPendingZone = pWorld->currentZone;
            pWorld->bHidePlayer = 1;
            nDragSlot = -1;
            bDragActive = 0;
            nDragLastScreenY = -1;
            nDragLastScreenX = -1;
            draggedTile = NULL;
            bBusy = 1;
            pWorld->completionCount++;
            CWinApp *pApp = AfxGetApp();
            pApp->WriteProfileInt("OPTIONS", "Count", pWorld->completionCount);
            int nZone = pWorld->GetVictoryZoneIndexMaybe();
            pWorld->nextCameraX = pWorld->cameraX;
            pWorld->nextCameraY = pWorld->cameraY;
            pWorld->cameraX = pWorld->cameraY = 0;
            pWorld->nFrameMode = 7;
            pWorld->nMapChangeReason = 2;
            pWorld->abortFrame = 0;
            PlaySound(0x3f);
            int i = 0;
            do
            {
                ZoneTransitionStep((short)nZone, (short)i);
                i++;
            } while (i < 11);
            bMouseCaptured = 0;
            HDC hdc = ::GetDC(m_hWnd);
            CDC *pDC = CDC::FromHandle(hdc);
            CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
            SetTextColor(pWorld->pCanvas->hdc, 0xffffff);
            int nOldBkMode = SetBkMode(pWorld->pCanvas->hdc, TRANSPARENT);
            pWorld->score = 0;
            pWorld->UpdateScore();
            if (pWorld->score < 1000)
                pWorld->score += rand() % 9;
            if (pWorld->score > 1000)
                pWorld->score = 1000;
            pWorld->lastScore = pWorld->score;
            if (pWorld->highScore < pWorld->lastScore)
                pWorld->highScore = pWorld->lastScore;
            pApp->WriteProfileInt("OPTIONS", "HScore", pWorld->highScore);
            pApp->WriteProfileInt("OPTIONS", "LScore", pWorld->lastScore);
            char szBuf[8];
            sprintf(szBuf, "%d", pWorld->score);
            HFONT hFont = CreateFont(-8, 0, 0, 0, 700, 0, 0, 0, 0, 0, 0, 0, 0, g_pszFontName);
            HGDIOBJ hOldFont = SelectObject(pWorld->pCanvas->hdc, hFont);
            TextOut(pWorld->pCanvas->hdc, 0xbe, 0xeb, szBuf, strlen(szBuf));
            SetBkMode(pWorld->pCanvas->hdc, nOldBkMode);
            DrawGameArea(pDC);
            DrawDirectionArrows(pDC);
            SelectObject(pWorld->pCanvas->hdc, hOldFont);
            pDC->SelectPalette(pOldPal, 0);
            ::ReleaseDC(m_hWnd, pDC->m_hDC);
            pWorld->nFrameMode = 7;
            pWorld->nMapChangeReason = 2;
            bMouseCaptured = 0;
            bInputLocked = 0;
            bBusy = 0;
            nTransitionStep = 0;
            return;
        }
        if (pWorld->abortFrame == -1)
        {
            CWinApp *pApp = AfxGetApp();
            pApp->GetProfileInt("OPTIONS", "LCount", pWorld->lastCount);
            pWorld->lastCount++;
            pApp->WriteProfileInt("OPTIONS", "LCount", pWorld->lastCount);
            pWorld->gameState = -1;
            nDragSlot = -1;
            bDragActive = 0;
            nDragLastScreenY = -1;
            nDragLastScreenX = -1;
            draggedTile = NULL;
            pWorld->bHidePlayer = 1;
            pWorld->pPendingZone = pWorld->currentZone;
            pWorld->currentZone = pWorld->GetLossZoneMaybe();
            pWorld->RefreshZone();
            pWorld->nextCameraX = pWorld->cameraX;
            pWorld->nextCameraY = pWorld->cameraY;
            pWorld->cameraX = pWorld->cameraY = 0;
            pWorld->UpdateCamera();
            DrawGameArea(NULL);
            pWorld->nFrameMode = 7;
            pWorld->nMapChangeReason = 3;
            pWorld->abortFrame = 0;
            PlaySound(0x3e);
            return;
        }
    }
    switch (pWorld->nFrameMode)
    {
    case 1:
        CyclePalette();
        break;
    case 2:
        CyclePalette();
        Tick();
        nWalkFramePhase++;
        if ((int)nWalkFramePhase > 3)
            nWalkFramePhase = 0;
        if (nMovePending != 0 && nMoveCommand != -1)
        {
            switch (nMoveCommand)
            {
            case 0x21:
                nMoveDX = 1;
                nMoveDY = -1;
                bMouseCaptured = 0;
                nMoveCommand = -1;
                nMovePending = 0;
                break;
            case 0x22:
                nMoveDX = 1;
                nMoveDY = 1;
                bMouseCaptured = 0;
                nMoveCommand = -1;
                nMovePending = 0;
                break;
            case 0x23:
                nMoveDX = -1;
                nMoveDY = 1;
                bMouseCaptured = 0;
                nMoveCommand = -1;
                nMovePending = 0;
                break;
            case 0x24:
                nMoveDX = -1;
                nMoveDY = -1;
                bMouseCaptured = 0;
                nMovePending = 0;
                nMoveCommand = -1;
                break;
            case 0x25:
                nMoveDX = -1;
                nMoveCommand = -1;
                bMouseCaptured = 0;
                nMovePending = 0;
                nMoveDY = 0;
                break;
            case 0x26:
                nMoveDX = 0;
                nMoveDY = -1;
                nMoveCommand = -1;
                nMovePending = 0;
                bMouseCaptured = 0;
                break;
            case 0x27:
                nMoveDX = 1;
                nMoveDY = 0;
                bMouseCaptured = 0;
                nMoveCommand = -1;
                nMovePending = 0;
                break;
            case 0x28:
                nMoveDX = 0;
                nMoveCommand = -1;
                nMovePending = 0;
                bMouseCaptured = 0;
                nMoveDY = 1;
                break;
            default:
                nMovePending = 0;
                nMoveCommand = -1;
                break;
            }
        }
        OnBumpTile(nMoveDX, nMoveDY);
        if (bMouseCaptured == 0)
            ReleaseCapture();
        break;
    case 3:
        CyclePalette();
        Tick();
        UpdatePlayerWalkFrame();
        if (bShowEmptyDialogOnceMaybe != 0)
        {
            CString strEmpty = "";
            ShowTextDialog(strEmpty, 0, 0, 0);
            bShowEmptyDialogOnceMaybe = 0;
        }
        UpdateItemObjectsMaybe();
        pWorld->currentZone->IactRun(1, pWorld->cameraX / 32, pWorld->cameraY / 32,
                                     0, 0, 0, NULL, pWorld, this);
        if (pWorld->currentZone->type == 6 || pWorld->currentZone->type == 7
            || pWorld->currentZone->type == 0xb)
        {
            TriggerHotspotsMaybe();
        }
        bMapAtCanvasOriginMaybe = 0;
        bSuppressWalkSound = 0;
        break;
    case 4:
        CyclePalette();
        UpdateDragCursor(0);
        break;
    case 5:
        bMouseCaptured = 0;
        break;
    case 6:
        switch (pWorld->nMapChangeReason)
        {
        case 1:
            ZoneTransitionStep((short)nTargetZoneId, (short)nTransitionStep);
            nTransitionStep++;
            if (nTransitionStep <= 10)
                return;
            pWorld->nFrameMode = 3;
            break;
        case 2:
            ZoneTransitionStep((short)nTargetZoneId, (short)nTransitionStep);
            nTransitionStep++;
            if (nTransitionStep <= 10)
                return;
            pWorld->nFrameMode = 3;
            break;
        case 3:
            if (nTransitionStep == 0 && pWorld->currentZone->globalVar >= 0)
            {
                Zone *pZone = pWorld->GetZoneById((short)nTargetZoneId);
                pZone->globalVar = pWorld->currentZone->globalVar;
            }
            ZoneTransitionStep((short)nTargetZoneId, (short)nTransitionStep);
            nTransitionStep++;
            if (nTransitionStep <= 10)
                return;
            pWorld->nFrameMode = 3;
            break;
        case 4:
            if (nTransitionStep == 0 && pWorld->currentZone->globalVar >= 0)
            {
                Zone *pZone = pWorld->GetZoneById((short)nTargetZoneId);
                pZone->globalVar = pWorld->currentZone->globalVar;
            }
            ZoneTransitionStep((short)nTargetZoneId, (short)nTransitionStep);
            nTransitionStep++;
            if (nTransitionStep <= 10)
                return;
            pWorld->nFrameMode = 3;
            break;
        default:
            return;
        case 8:
            if (nTransitionStep == 0 && pWorld->currentZone->globalVar >= 0)
            {
                Zone *pZone = pWorld->GetZoneById((short)nTargetZoneId);
                pZone->globalVar = pWorld->currentZone->globalVar;
            }
            if (ZoneTransitionStep((short)nTargetZoneId, (short)nTransitionStep) != 0)
                nTransitionStep++;
            if (nTransitionStep <= 10)
                return;
            pWorld->nFrameMode = 3;
            break;
        }
        bBusy = 0;
        break;
    case 7:
        if (pWorld->nMapChangeReason == 2)
        {
            nTransitionStep++;
            if (nTransitionStep > 0x33)
                nTransitionStep = 0x33;
        }
        else if (pWorld->nMapChangeReason == 4)
        {
            nTransitionStep++;
            if (nTransitionStep > 2)
            {
                bBlinkState = bBlinkState == 0;
                HDC hdc = ::GetDC(m_hWnd);
                CDC *pDC = CDC::FromHandle(hdc);
                CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
                pWorld->DrawLocatorMap(pDC, bBlinkState, bMapTeleportEnabled);
                pDC->SelectPalette(pOldPal, 0);
                ::ReleaseDC(m_hWnd, pDC->m_hDC);
                nTransitionStep = 0;
            }
        }
        break;
    case 8:
        CyclePalette();
        Tick();
        FireWeaponStep(nFireStep);
        nFireStep++;
        if (nFireStep > 3 && pWorld->nFrameMode != 9)
            pWorld->nFrameMode = 3;
        break;
    case 9:
        CyclePalette();
        nTransitionStep++;
        pWorld->DrawPlayer();
        if (nTransitionStep > 1)
        {
            nTransitionStep = 0;
            bBlinkState = bBlinkState == 0;
            if (bBlinkState != 0)
            {
                BlitTile((short)nPickupY, (short)nPickupX, 2,
                         (Tile *)pWorld->tiles.GetAt(nPickupTileId));
                DrawGameArea(NULL);
            }
            else
            {
                DrawTileAt((short)nPickupX, (short)nPickupY, -1);
                DrawGameArea(NULL);
            }
        }
        break;
    case 0xb:
        if (nTransitionStep == 0 && g_bReplayMode == 1)
        {
            pWorld->OnLoadWorld();
            break;
        }
        if (nTransitionStep == 0)
        {
            if (pWorld->bWorldInvalid == 0)
                bSuppressWalkSound = 1;
            bDragActive = 0;
            nDragSlot = -1;
            nDragLastScreenY = -1;
            nDragLastScreenX = -1;
            draggedTile = NULL;
            if (pWorld->bWorldInvalid != 0)
            {
                DrawWeaponBox(NULL);
                DrawWeaponIcon(NULL);
            }
            ::SetScrollRange(pInvScrollBar->m_hWnd, 2, 0, 1, 0);
            pInvScrollBar->scrollMax = 0;
            pInvScrollBar->scrollPos = 0;
            ::SetScrollPos(pInvScrollBar->m_hWnd, 2, 0, 1);
            bMapViewOpen = 0;
            DrawText(NULL);
            DrawHealthNeedle(NULL);
            if (pWorld->bWorldInvalid != 0)
            {
                int nRange = pWorld->inventory.GetSize();
                if (nRange > 7)
                {
                    nRange -= 7;
                    ::SetScrollRange(pInvScrollBar->m_hWnd, 2, 0, nRange, 1);
                    pInvScrollBar->scrollMax = nRange;
                }
                else
                {
                    ::SetScrollRange(pInvScrollBar->m_hWnd, 2, 0, 1, 1);
                    pInvScrollBar->scrollMax = 0;
                }
            }
            pWorld->bHidePlayer = 0;
            pWorld->bWorldReadyMaybe = 0;
        }
        if (pWorld->bWorldInvalid == 0 && pWorld->totalZones != -1)
            WorldEntryStepMaybe((short)nTargetZoneId, (short)nTransitionStep);
        else
            ZoneTransitionStep((short)nTargetZoneId, (short)nTransitionStep);
        nTransitionStep++;
        if (nTransitionStep > 10)
        {
            pWorld->nFrameMode = 3;
            bBusy = 0;
            DrawDirectionArrows(NULL);
            pWorld->bWorldInvalid = 0;
            if (pWorld->nFrameDelay == 0x1e && bOneShotStubMaybe == 0)
            {
                bOneShotStubMaybe = 1;
                EmptyFrameHookMaybe();
            }
        }
        break;
    case 0xc:
    case 0xd:
        {
            CString strMsg;
            pWorld->OnCloseDocument();
            strMsg.LoadString(0xe01e);
            FatalAppExit(0, strMsg);
        }
        break;
    case 0xe:
        bBusy = 1;
        if (bPauseOverlayDrawn == 0)
        {
            BlitViewportDither();
            bPauseOverlayDrawn = 1;
        }
        bBusy = 0;
        break;
    }
}

// FUNCTION: YODA 0x0040e400
// GameView::StepDetonatorEffect — thermal-detonator explosion animation at
// (nDetonatorX,nDetonatorY): phases 0..2 blit tiles 0x202/0x431/0x432, phase 3
// blits 0x433 then DetonateAdjacentTiles, phase 4 restores the zone cell.
// [dial-breather: EXACT v22 -> out at the Canvas.h de-dup (2026-07-07), swapped with
//  ReenableHotspotObjects/UpdatePlayerWalkFrame flipping in. Tie-break only; G1.]
void CDeskcppView::StepDetonatorEffect()
{
    int x = nDetonatorX;
    int y = nDetonatorY;

    switch (nDetonatorPhase)
    {
    case 0:
        BlitTile((short)y, (short)x, 0, pWorld->GetTileData(0x202));
        DrawGameArea(NULL);
        return;
    case 1:
        BlitTile((short)y, (short)x, 0, pWorld->GetTileData(0x431));
        DrawGameArea(NULL);
        return;
    case 2:
        BlitTile((short)y, (short)x, 0, pWorld->GetTileData(0x432));
        DrawGameArea(NULL);
        return;
    case 3:
        BlitTile((short)y, (short)x, 0, pWorld->GetTileData(0x433));
        DrawGameArea(NULL);
        DetonateAdjacentTiles(x, y);
        return;
    case 4:
        DrawZoneCell((short)x, (short)y);
        DrawGameArea(NULL);
        break;
    }
}

// FUNCTION: YODA 0x0040e500
// GameView::ApplyHotspotCamera — vehicle hotspot click: pObj is an active
// OBJ_VEHICLE_TO/FROM object; find the paired object in the linked zone
// (pObj->arg), aim the camera at it (<<5 = tile->pixel) and kick off a
// mode-6 zone transition (nMapChangeReason 1 = to, 2 = from). Returns 1 if
// the transition was started.
// EFFECTIVE MATCH (align=34, 168/168 insns, byte_diff=31): the two type arms
// are a textual clone pair and the ORIGINAL emits them parity-CROSSED
// (block1: vx's xor folded into the GetZoneById call setup, i's xor after the
// count load; block2: the reverse — i pre-call, vx post-count) — identical
// source cannot produce both (ZTS<->WES / loader-triplet family, G1).
// Matched block1's registers exactly (i=EDX, walker=ECX, count reg-resident);
// residual = the count-load/i-xor 2-insn order in block1 + block2's rotated
// roles (EBX/EBP/ECX/EDX cycle) with cmp-direction fallout. Probed: i-vs-
// nCount decl order x3 (seesaw between blocks), decl-early/init-late i,
// vx-before/after-pZone (after = -20 align, kept). Structure cracks that
// landed: Find-fail arm as else-of-(!=0) (fail body sunk after the then-arm),
// vx=0 declared right after the pZone call (folds into call setup).
int CDeskcppView::ApplyHotspotCamera(ZoneObj *pObj)
{
    int cellX;
    int cellY;

    if (pObj->state == 0)
        return 0;
    bBusy = 1;
    if (pObj->type == OBJ_VEHICLE_TO)
    {
        int nZoneId = pObj->arg;
        if (pWorld->FindZoneCellById((short)nZoneId, &cellX, &cellY) != 0)
        {
            Zone *pZone = pWorld->GetZoneById((short)nZoneId);
            int vx = 0;
            if (pZone != NULL)
            {
                int i = 0;
                int nCount = pZone->objects.GetSize();
                int vy = 0;
                int bFound = 0;
                if (nCount > 0)
                {
                    do
                    {
                        ZoneObj *pO = (ZoneObj *)pZone->objects.GetAt(i);
                        if (pO->type == OBJ_VEHICLE_FROM)
                        {
                            bFound = 1;
                            vx = pO->x;
                            vy = pO->y;
                            break;
                        }
                        i++;
                    } while (i < nCount);
                }
                if (bFound != 0)
                {
                    if (pZone->activatedFlag == 0)
                        pWorld->PlaceZoneObjectTiles((short)nZoneId);
                    pWorld->cameraX = vx << 5;
                    pWorld->cameraY = vy << 5;
                    pWorld->nMapChangeReason = 1;
                    nTargetZoneId = nZoneId;
                    nTransitionStep = 0;
                    pWorld->nFrameMode = 6;
                    bBusy = 0;
                    return 1;
                }
            }
        }
        else
        {
            bBusy = 0;
            return 0;
        }
    }
    else if (pObj->type == OBJ_VEHICLE_FROM)
    {
        int nZoneId = pObj->arg;
        if (pWorld->FindZoneCellById((short)nZoneId, &cellX, &cellY) != 0)
        {
            Zone *pZone = pWorld->GetZoneById((short)nZoneId);
            int vx = 0;
            if (pZone != NULL)
            {
                int i = 0;
                int nCount = pZone->objects.GetSize();
                int vy = 0;
                int bFound = 0;
                if (nCount > 0)
                {
                    do
                    {
                        ZoneObj *pO = (ZoneObj *)pZone->objects.GetAt(i);
                        if (pO->type == OBJ_VEHICLE_TO)
                        {
                            bFound = 1;
                            vx = pO->x;
                            vy = pO->y;
                            break;
                        }
                        i++;
                    } while (i < nCount);
                }
                if (bFound != 0)
                {
                    pWorld->cameraX = vx << 5;
                    pWorld->cameraY = vy << 5;
                    pWorld->nMapChangeReason = 2;
                    nTargetZoneId = nZoneId;
                    nTransitionStep = 0;
                    pWorld->nFrameMode = 6;
                    bBusy = 0;
                    return 1;
                }
            }
        }
        else
        {
            bBusy = 0;
            return 0;
        }
    }
    bBusy = 0;
    return 0;
}

// FUNCTION: YODA 0x0040e750
// GameView::TransitionZoneScript — IACT-driven zone change (command warp):
// activate the target zone's objects if needed, then start the mode-6
// transition with nMapChangeReason 8. arg1 is pushed by callers but never
// read (sig byte-proven: ret 8, plain dword loads).
int CDeskcppView::TransitionZoneScript(int nUnused, int nZoneId)
{
    bBusy = 1;
    Zone *pZone = pWorld->GetZoneById((short)nZoneId);
    if (pZone != NULL)
    {
        if (pZone->activatedFlag == 0)
            pWorld->PlaceZoneObjectTiles((short)nZoneId);
        pWorld->nMapChangeReason = 8;
        nTargetZoneId = nZoneId;
        nTransitionStep = 0;
        pWorld->nFrameMode = 6;
        bInputLocked = 0;
        bBusy = 0;
        return 1;
    }
    bBusy = 0;
    return 0;
}

// FUNCTION: YODA 0x0040e7c0
// GameView::TransitionZoneXWing — X-Wing takeoff/landing hotspot: like
// ApplyHotspotCamera but for the OBJ_XWING_TO/FROM pair, and the world map
// is swapped: TO (takeoff) backs up the zone grid, builds the fresh
// travel grid and restores the quest records (bQuestCellsResident=1); FROM (landing)
// backs up the records and restores the grid backup (bQuestCellsResident=0).
// EFFECTIVE MATCH (align=22, 152/152 insns, byte_diff=10): same parity-
// crossed clone pair as ApplyHotspotCamera (the pre-call xor is vx in arm1
// but i in arm2 in the ORIGINAL). Arm1 matches exactly. Arm2 residual = a
// 2-reg i<->count cycle (orig i=EBX,count=EDI; ours i=EDI,count=EBX) + the
// vx/vy xor scheduling around the count load + cmp-direction fallout.
// Probes: identical-clone decls (align=24, reg_pen=3 — the plausible true
// source, G1 candidate), i-after-pZone + vx/vy inside (kept, align=22),
// vx-before-nCount (32k), i-before-pZone (43k).
int CDeskcppView::TransitionZoneXWing(ZoneObj *pObj)
{
    if (pObj->state == 0)
        return 0;
    bBusy = 1;
    if (pObj->type == OBJ_XWING_TO)
    {
        int nZoneId = pObj->arg;
        Zone *pZone = pWorld->GetZoneById((short)nZoneId);
        int vx = 0;
        if (pZone != NULL)
        {
            int nCount = pZone->objects.GetSize();
            int i = 0;
            int vy = 0;
            int bFound = 0;
            if (nCount > 0)
            {
                do
                {
                    ZoneObj *pO = (ZoneObj *)pZone->objects.GetAt(i);
                    if (pO->type == OBJ_XWING_FROM)
                    {
                        bFound = 1;
                        vx = pO->x;
                        vy = pO->y;
                        break;
                    }
                    i++;
                } while (i < nCount);
            }
            if (bFound != 0)
            {
                if (pZone->activatedFlag == 0)
                    pWorld->PlaceZoneObjectTiles((short)nZoneId);
                pWorld->cameraX = vx << 5;
                pWorld->cameraY = vy << 5;
                pWorld->nMapChangeReason = 1;
                nTransitionStep = 0;
                nTargetZoneId = nZoneId;
                pWorld->nFrameMode = 6;
                pWorld->BackupZoneGrid();
                pWorld->SetupGrid();
                pWorld->RestoreRecords();
                pWorld->bQuestCellsResident = 1;
                bBusy = 0;
                return 1;
            }
        }
    }
    else if (pObj->type == OBJ_XWING_FROM)
    {
        int nZoneId = pObj->arg;
        Zone *pZone = pWorld->GetZoneById((short)nZoneId);
        int i = 0;
        if (pZone != NULL)
        {
            int nCount = pZone->objects.GetSize();
            int vx = 0;
            int vy = 0;
            int bFound = 0;
            if (nCount > 0)
            {
                do
                {
                    ZoneObj *pO = (ZoneObj *)pZone->objects.GetAt(i);
                    if (pO->type == OBJ_XWING_TO)
                    {
                        bFound = 1;
                        vx = pO->x;
                        vy = pO->y;
                        break;
                    }
                    i++;
                } while (i < nCount);
            }
            if (bFound != 0)
            {
                pWorld->cameraX = vx << 5;
                pWorld->cameraY = vy << 5;
                pWorld->nMapChangeReason = 2;
                nTargetZoneId = nZoneId;
                nTransitionStep = 0;
                pWorld->nFrameMode = 6;
                pWorld->BackupRecords();
                pWorld->SetupGrid();
                pWorld->RestoreGridFromBackup();
                pWorld->bQuestCellsResident = 0;
                bBusy = 0;
                return 1;
            }
        }
    }
    bBusy = 0;
    return 0;
}

// FUNCTION: YODA 0x0040e9d0
// GameView::TransitionZoneDoor — walk-through-door transition (OnBumpTile).
// DOOR_IN: record the return position into the current zone
// (doorReturnX/Y), stamp the paired DOOR_OUT's arg with the current zone
// index, aim the camera at the DOOR_OUT and start the mode-6 transition
// (reason 3). DOOR_OUT: return to the zone in arg at its saved
// doorReturnX/Y (reason 4).
// EFFECTIVE MATCH (align=22, 151/151 insns, reg_pen=0, byte_diff=10): sole
// residual = ONE instruction position — the orig folds vx's `xor ebx,ebx`
// INSIDE GetZoneIndex's argument evaluation (between the pWorld load and the
// currentZone deref); ours emits it at pDoor(EBX)'s death 3 insns earlier.
// vx-decl before/after the call statement both inert. Every register/slot
// identical. Scheduling family, G1.
void CDeskcppView::TransitionZoneDoor(ZoneObj *pDoor)
{
    if (pDoor->state != 0)
    {
        bBusy = 1;
        if (pDoor->type == OBJ_DOOR_IN)
        {
            int nZoneId = pDoor->arg;
            if (nZoneId < 0)
            {
                bBusy = 0;
                return;
            }
            Zone *pZone = pWorld->GetZoneById((short)nZoneId);
            if (pZone == NULL)
            {
                bBusy = 0;
                return;
            }
            pWorld->currentZone->doorReturnX = pDoor->x;
            pWorld->currentZone->doorReturnY = pDoor->y;
            int nCurZoneIdx = pWorld->GetZoneIndex(pWorld->currentZone);
            int vx = 0;
            int vy = 0;
            int bFound = 0;
            int nCount = pZone->objects.GetSize();
            int i = 0;
            if (nCount > 0)
            {
                do
                {
                    ZoneObj *pO = (ZoneObj *)pZone->objects.GetAt(i);
                    if (pO->type == OBJ_DOOR_OUT)
                    {
                        bFound = 1;
                        pO->arg = (short)nCurZoneIdx;
                        vx = pO->x;
                        vy = pO->y;
                        break;
                    }
                    i++;
                } while (i < nCount);
            }
            if (bFound != 0)
            {
                if (pZone->activatedFlag == 0)
                    pWorld->PlaceZoneObjectTiles((short)nZoneId);
                pWorld->cameraX = vx << 5;
                pWorld->cameraY = vy << 5;
                pWorld->nMapChangeReason = 3;
                nTargetZoneId = nZoneId;
                nTransitionStep = 0;
                pWorld->nFrameMode = 6;
                bBusy = 0;
                return;
            }
        }
        else if (pDoor->type == OBJ_DOOR_OUT)
        {
            int nZoneId = pDoor->arg;
            if (nZoneId < 0)
            {
                bBusy = 0;
                return;
            }
            Zone *pZone = pWorld->GetZoneById((short)nZoneId);
            if (pZone == NULL)
            {
                bBusy = 0;
                return;
            }
            int y2 = pZone->doorReturnY;
            pWorld->cameraX = pZone->doorReturnX << 5;
            pWorld->cameraY = y2 << 5;
            pWorld->nMapChangeReason = 4;
            nTargetZoneId = nZoneId;
            nTransitionStep = 0;
            pWorld->nFrameMode = 6;
            bBusy = 0;
            return;
        }
        bBusy = 0;
    }
}

// FUNCTION: YODA 0x0040ebe0
// GameView::ReenableHotspotObjects — re-arm all vehicle hotspots in the
// current zone (state=1) so they can be triggered again.
void CDeskcppView::ReenableHotspotObjects()
{
    int n = pWorld->currentZone->objects.GetSize();
    if (n > 0)
    {
        int i = 0;
        do
        {
            ZoneObj *pO = (ZoneObj *)pWorld->currentZone->objects.GetAt(i);
            if (pO->type == OBJ_VEHICLE_TO || pO->type == OBJ_VEHICLE_FROM)
                pO->state = 1;
            i++;
            n--;
        } while (n != 0);
    }
}

// FUNCTION: YODA 0x0040ec30
// GameView::TriggerHotspotsMaybe — scan the current zone's objects for an
// ACTIVE vehicle/X-Wing hotspot on the camera tile (cameraX/Y are pixel
// coords, /32 = tile) and fire its transition. Returns the transition
// call's result (0 if nothing fired). Skipped entirely while a mode-6/7
// transition is already running.
// EFFECTIVE MATCH (126/130 insns; align inflated by the 13-entry jump
// table disassembling as data): body identical except (a) pWorld/nCount
// ride ESI/ECX in the orig vs ECX/ESI in ours — one 2-reg role swap that
// also flips the 2-insn prologue order (orig loads pWorld before this->EDI)
// — and (b) i's xor vs n's spill order at loop entry. World* local probe
// inert (CSE identical); n/i decl order inert. Countdown recipe (separate
// nCount guard + n counter) and per-arm duplicated calls (vehicle pair kept
// separate, xwing pair cross-jumped by cl) both required.
int CDeskcppView::TriggerHotspotsMaybe()
{
    int nResult = 0;

    if (pWorld->nFrameMode == 6 || pWorld->nFrameMode == 7)
        return 0;
    int nCount = pWorld->currentZone->objects.GetSize();
    if (nCount == 0)
        return 0;
    bBusy = 1;
    int tx = pWorld->cameraX / 32;
    int ty = pWorld->cameraY / 32;
    if (nCount > 0)
    {
        int n = nCount;
        int i = 0;
        do
        {
            ZoneObj *pO = (ZoneObj *)pWorld->currentZone->objects.GetAt(i);
            if (pO->state != 0)
            {
                switch (pO->type)
                {
                case OBJ_VEHICLE_TO:
                    if (pO->x == tx && pO->y == ty)
                        nResult = ApplyHotspotCamera(pO);
                    break;
                case OBJ_VEHICLE_FROM:
                    if (pO->x == tx && pO->y == ty)
                        nResult = ApplyHotspotCamera(pO);
                    break;
                case OBJ_XWING_FROM:
                    if (pO->x == tx && pO->y == ty)
                        nResult = TransitionZoneXWing(pO);
                    break;
                case OBJ_XWING_TO:
                    if (pO->x == tx && pO->y == ty)
                        nResult = TransitionZoneXWing(pO);
                    break;
                }
            }
            i++;
            n--;
        } while (n != 0);
    }
    bBusy = 0;
    return nResult;
}

// FUNCTION: YODA 0x0040ed90
// GameView::UpdateItemObjectsMaybe — per-frame item-object pass over the
// current zone (was "DrawMap"): for pickup-able objects (quest item spot /
// Force / item / weapon; locator with its own flag mask) standing under the
// camera tile, move the tile into the inventory and clear it; for objects
// whose layer-1 tile has gone missing, re-place it from pO->arg (spawns
// re-place from the map cell's quest slot). Redraws the touched cell.
// sic: in the quest-item arm a vanished tile / non-item tile ABORTS the
// whole pass (early return), while the locator arm just skips.
// EFFECTIVE MATCH (align residual; 224 real insns aligned 1:1, byte_diff=115
// mostly reloc/table-offset shift): body structure EXACT including the
// 16-entry byte jump table. Residuals: (a) this/pO reg-role 2-cycle (orig
// this=EDI,pO=EBX; ours EBX/EDI) which also shifts one prologue push ebp —
// flipped when the empty case arm was added, tie-break; (b) the quest-arm
// flags test: orig loads flags to ECX + test cl,0xc0, ours folds to
// test byte[mem] — SAME axis as FireWeaponStep's parked flags-test (a
// local is copy-propagated away, single-use); (c) tx/ty cmp slot-vs-reg
// direction (operand-flip probe inert, lesson #6).
// ⭐ NEW MECHANISM (proven via the dword table at 0x40f024): cl 4.2 assigns
// a jump-table ARM INDEX PER CASE LABEL VALUE (dense, value-ordered), so
// grouped labels (case 0: case 2: case 6: case 8:) get 4 DISTINCT indices
// all pointing at the shared arm address, and an empty `case 4: case 10:
// case 15: break;` arm widens the table to 16 entries with its indices
// aiming at the exit block. Duplicating the shared bodies textually does
// NOT reproduce this (cl 4.2 never folds duplicate arms - +155 insns).
// Other cracks: int t = (short)GetTile(...) temps (movsx-immediately),
// DRAW tail (DrawTileAt+DrawGameArea) textually duplicated per arm
// (cross-jumped), i decl AFTER ty (xor interleaves into ty's division).
void CDeskcppView::UpdateItemObjectsMaybe()
{
    if (pWorld->nFrameMode == 6 || pWorld->nFrameMode == 7)
        return;
    int nCount = pWorld->currentZone->objects.GetSize();
    if (nCount != 0)
    {
        bBusy = 1;
        int tx = pWorld->cameraX / 32;
        int ty = pWorld->cameraY / 32;
        int i = 0;
        if (nCount > 0)
        {
            do
            {
                Zone *pZone = pWorld->currentZone;
                ZoneObj *pO = (ZoneObj *)pZone->objects.GetAt(i);
                if (pO->state != 0)
                {
                    switch (pO->type)
                    {
                    case OBJ_QUEST_ITEM_SPOT:
                    case OBJ_THE_FORCE:
                    case OBJ_ITEM:
                    case OBJ_WEAPON:
                        if (pO->x == tx && pO->y == ty)
                        {
                            int t = (short)pZone->GetTile(pO->x, pO->y, 1);
                            if (t < 0)
                            {
                                bBusy = 0;
                                return;
                            }
                            Tile *pTile = (Tile *)pWorld->tiles.GetAt(t);
                            unsigned int flags = pTile->flags;
                            if ((flags & 0xc0) == 0)
                            {
                                bBusy = 0;
                                return;
                            }
                            AddItemToInv(pTile);
                            pWorld->currentZone->SetTile(pO->x, pO->y, 1, -1);
                            pO->state = 0;
                            DrawTileAt(pO->x, pO->y, -1);
                            DrawGameArea(NULL);
                        }
                        else
                        {
                            int t = (short)pZone->GetTile(pO->x, pO->y, 1);
                            if (t < 0)
                            {
                                pWorld->currentZone->SetTile(pO->x, pO->y, 1, pO->arg);
                                DrawTileAt(pO->x, pO->y, -1);
                                DrawGameArea(NULL);
                            }
                        }
                        break;
                    case OBJ_SPAWN:
                        {
                            int t = (short)pZone->GetTile(pO->x, pO->y, 1);
                            if (t < 0)
                            {
                                short cell = pWorld->mapGrid[pWorld->playerY * 10 + pWorld->playerX].cellQuestSlot6;
                                if (cell >= 0)
                                {
                                    pWorld->currentZone->SetTile(pO->x, pO->y, 1, cell);
                                    DrawTileAt(pO->x, pO->y, -1);
                                    DrawGameArea(NULL);
                                }
                            }
                        }
                        break;
                    case OBJ_VEHICLE_FROM:
                    case OBJ_DOOR_OUT:
                    case OBJ_XWING_TO:
                        break;
                    case OBJ_LOCATOR:
                        if (pO->x == tx && pO->y == ty)
                        {
                            int t = (short)pZone->GetTile(pO->x, pO->y, 1);
                            if (t >= 0)
                            {
                                Tile *pTile = (Tile *)pWorld->tiles.GetAt(t);
                                if ((pTile->flags & 0x100080) != 0)
                                {
                                    AddItemToInv(pTile);
                                    pWorld->currentZone->SetTile(pO->x, pO->y, 1, -1);
                                    pO->state = 0;
                                    DrawTileAt(pO->x, pO->y, -1);
                                    DrawGameArea(NULL);
                                }
                            }
                        }
                        else
                        {
                            int t = (short)pZone->GetTile(pO->x, pO->y, 1);
                            if (t < 0)
                            {
                                pWorld->currentZone->SetTile(pO->x, pO->y, 1, pO->arg);
                                DrawTileAt(pO->x, pO->y, -1);
                                DrawGameArea(NULL);
                            }
                        }
                        break;
                    }
                }
                i++;
            } while (nCount > i);
        }
        bBusy = 0;
    }
}

// FUNCTION: YODA 0x0040f060  (?DrawTextA@GameView@@QAEXPAVCDC@@@Z — windows.h renames DrawText)
// GameView::DrawText — paints the 7-slot inventory panel (rectUnk3284):
// for each visible row, fill the 32x32 drag canvas with the button-face
// color, blit the item tile into it (skipped for the slot being dragged),
// frame the icon cell + name cell with the bevel DrawRect, blit the icon,
// and TextOut the item name in MS Sans Serif. pDC==NULL = self-service
// GetDC + palette select (bReleaseDC arm).
// EFFECTIVE MATCH (285/284 insns, align=172 = one coherent reg-role
// rotation echoed ~30x + slot-order fallout): every structural element
// aligned — the EH frame, the cached SelectObject vcall (vtbl/fnptr slots
// -0x44/-0x3c), the inventory byte-offset walker, rc/rc2 slots, all call
// shapes. Residual: pTile rides EBX (orig EDI), freeing EDI for a pWorld
// CSE the orig doesn't do (2-insn delta), + this/pDC reload role noise.
// pItem/pTile decl order inert. CRACKS (new lessons): (a) MFC 4.2's ONLY
// virtual CDC::SelectObject overload is (CFont*) — the +0x30 vcall proves
// the source used CFont::FromHandle (CGdiObject* selects the non-virtual
// INLINE overload -> m_hObject extraction, wrong shape); write the
// FromHandle result to a local first (evaluate-callee-first). (b) the
// GetAt walker needed a dedicated `int slot = nScroll;` IV (SR to
// scroll<<2 byte walker) while the bounds tests spell nScroll + i.
// (c) windows.h renames DrawText->DrawTextA: marker carries the mangled
// hint, asmscore now parses it.
void CDeskcppView::DrawText(CDC *pDC)
{
    int bReleaseDC = 0;
    CPalette *pOldPal;

    if (pDC == NULL)
    {
        pDC = GetDC();
        if (pDC == NULL)
            return;
        pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
        bReleaseDC = 1;
    }
    HFONT hFont = CreateFont(-14, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, g_pszFontName);
    CFont *pFont = CFont::FromHandle(hFont);
    CFont *pOldFont = pDC->SelectObject(pFont);
    int nCount = pWorld->inventory.GetSize();
    pDC->SetBkMode(1);
    int nScroll = pInvScrollBar->scrollPos;
    if (nScroll < 0)
        nScroll = 0;
    int i = 0;
    int y = 0;
    int slot = nScroll;
    RECT rc;
    do
    {
        Tile *pTile;
        InvItem *pItem;
        if (nScroll + i < nCount)
        {
            pItem = (InvItem *)pWorld->inventory.GetAt(slot);
            pTile = pItem->pTile;
            pDragTileCanvas->Fill((char)GetNearestPaletteIndex((HPALETTE)pWorld->pPalette->m_hObject, GetSysColor(0xf)));
            if (nDragSlot - i != nScroll)
                pDragTileCanvas->BlitMasked((char *)pTile->pixels, 0x20, 0x20, 0, 0, 0);
        }
        CBrush brush(GetSysColor(0xf));
        CBrush *pOldBrush = pDC->SelectObject(&brush);
        rc.top = pWorld->rectUnk3284.top + y;
        rc.bottom = rc.top + 0x20;
        rc.left = pWorld->rectUnk3284.left;
        rc.right = rc.left + 0x20;
        if (nCount <= nScroll + i)
            PatBlt(pDC->m_hDC, rc.left, rc.top, 0x20, 0x20, 0xf00021);
        pWorld->DrawRect(pDC, &rc, 1, 1);
        if (nScroll + i < nCount)
            pDragTileCanvas->BitBlt(pDC, rc.left + 1, rc.top + 1, 0x1e, 0x1e, 1, 1);
        rc.left += 0x21;
        rc.right = pWorld->rectUnk3284.right;
        RECT rc2;
        CopyRect(&rc2, &rc);
        PatBlt(pDC->m_hDC, rc2.left, rc2.top, rc2.right - rc2.left, rc2.bottom - rc2.top, 0xf00021);
        pDC->SelectObject(pOldBrush);
        pWorld->DrawRect(pDC, &rc, 1, 1);
        if (nScroll + i < nCount)
            pDC->TextOut(rc.left + 4, rc.top + 8, pItem->name);
        y += 0x20;
        slot++;
        i++;
    } while (y < 0xe0);
    rc.top = pWorld->rectUnk3284.top;
    rc.left = pWorld->rectUnk3284.left;
    rc.right = rc.left + 0x20;
    rc.bottom = rc.top + 0x20;
    pWorld->DrawRect(pDC, &rc, 1, 1);
    pDC->SelectObject(pOldFont);
    DeleteObject(hFont);
    if (bReleaseDC != 0)
    {
        pDC->SelectPalette(pOldPal, 0);
        ReleaseDC(pDC);
    }
}

// FUNCTION: YODA 0x0040f3d0  (??1CBrush@@UAE@XZ — TU COMDAT copy, emitted by DrawText's local CBrush)
// FUNCTION: YODA 0x0040f420  (??_GCBrush@@UAEPAXI@Z — scalar-deleting dtor, same origin)

// FUNCTION: YODA 0x0040f4b0
// GameView::ShowWinMessage — puzzle-solving interaction at the bumped cell
// (x,y)+(dx,dy). Three arms on the equipped item: tile 780 (the demo's goal
// item) in a FIND_USEFUL_DROP cell, tile 2034 with goalItemTileId 0xbd
// (dialog placed at the camera instead of the bumped tile), else the
// generic NPC trade (cellQuestSlot0 indexes the quest word lists; the
// cell's field30 picks list A vs B). Unsolved cells (flagA==0) show the
// reward text (text4 / text1+" "+next text3), blit the reward item and
// enter the mode-9 pickup blink; solved cells re-show text3/text2 (with
// the "you won" string 0xe00b overriding after victory).
// EFFECTIVE-WIP (495/491 insns, all six CString EH scopes + every call and
// store aligned; align=654 dominated by a global allocation split):
// (a) the orig HOMES tx/ty to slots [-0x1c]/[-0x14] and pre-loads
// playerX/playerY/equippedItem/tiles.m_pData into EDX/EDI/ESI/EAX above the
// arm dispatch (CSE'd across all 3 arm conditions; arm B's cmp reuses
// EAX/ESI from the head); ours gives tx/ty the callee-saved regs and spills
// m_pData/equipped to slots instead — one global rank tie-break echoed
// everywhere. Probed: no-tx/ty-locals full-expression form (worse — the
// orig computes both at HEAD, so they ARE source locals), n operand order
// (canonicalized, inert). (b) the orig's paired field30 if/else arms have
// CROSSED size/data reg roles (A: size->EAX,data->ESI; B: size->ESI,
// data->EAX) which blocks cl's cross-jump of the GetAt tails; ours emits
// them symmetric and merges — same intra-function rotation family as the
// ZTS/WES clone pairs, not source-steerable. Cracks that landed: int id
// locals (xor+mov dx zero-extend idiom, v10 lesson), int a/b hoisted only
// in arm C (both bodies), per-arm duplicated dismiss-flag if/else,
// str += " " via the 0x456108 literal (adjacent to the yen/cent
// placeholder glyphs in this TU's literal pool).
void CDeskcppView::ShowWinMessage(int x, int y, int dx, int dy)
{
    int tx = dx + x;
    int ty = dy + y;

    if (YODA_SIC_FIX((pWorld->tiles.GetSize() > 780 || (BUGLOG(("sic#16 ShowWinMessage: tile 780 idx OOB n=%d\n", (int)pWorld->tiles.GetSize())), 0)) &&) (Tile *)pWorld->tiles.GetAt(780) == pWorld->equippedItem)
    {
        int n = pWorld->playerY * 10 + pWorld->playerX;
        if (pWorld->mapGrid[n].zoneType == ZONE_TYPE_FIND_USEFUL_DROP)
        {
            if (pWorld->mapGrid[n].flagA == 0)
            {
                int id;
                if (pWorld->mapGrid[n].field30 == 1)
                    id = pWorld->questItemsA.GetAt(pWorld->questItemsA.GetSize() - 1);
                else
                    id = pWorld->questItemsB.GetAt(pWorld->questItemsB.GetSize() - 1);
                CString str = ((Puzzle *)pWorld->puzzles.GetAt(id))->text4;
                pWorld->DrawPlayer();
                DrawGameArea(NULL);
                ShowTextDialog(str, tx * 32 + 16, ty * 32 + 16, 0);
                pWorld->mapGrid[n].flagA = 1;
                int id2;
                if (pWorld->mapGrid[n].field30 == 1)
                    id2 = pWorld->questItemsA.GetAt(0);
                else
                    id2 = pWorld->questItemsB.GetAt(0);
                int nTile = ((Puzzle *)pWorld->puzzles.GetAt(id2))->itemA;
                BlitTile((short)ty, (short)tx, 2, (Tile *)pWorld->tiles.GetAt(nTile));
                DrawGameArea(NULL);
                nPickupX = tx;
                nPickupY = ty;
                nPickupTileId = nTile;
                pPickupObj = NULL;
                nTransitionStep = 0;
                bBlinkState = 0;
                pWorld->nFrameMode = 9;
                bMouseCaptured = 0;
                pWorld->mapGrid[n].flagB = 1;
            }
            else
            {
                int id;
                if (pWorld->mapGrid[n].field30 == 1)
                    id = pWorld->questItemsA.GetAt(pWorld->questItemsA.GetSize() - 1);
                else
                    id = pWorld->questItemsB.GetAt(pWorld->questItemsB.GetSize() - 1);
                CString str = ((Puzzle *)pWorld->puzzles.GetAt(id))->text3;
                pWorld->DrawPlayer();
                DrawGameArea(NULL);
                if (pWorld->gameState == 1)
                    str.LoadString(0xe00b);
                ShowTextDialog(str, tx * 32 + 16, ty * 32 + 16, 0);
                if (bDialogClickDismissMaybe == 0)
                {
                    bTextDialogShown = 1;
                    bInputLocked = 1;
                }
                else
                {
                    bTextDialogShown = 1;
                    bInputLocked = 0;
                }
            }
        }
    }
    else if (YODA_SIC_FIX((pWorld->tiles.GetSize() > 2034 || (BUGLOG(("sic#16 ShowWinMessage: tile 2034 idx OOB n=%d\n", (int)pWorld->tiles.GetSize())), 0)) &&) (Tile *)pWorld->tiles.GetAt(2034) == pWorld->equippedItem && pWorld->goalItemTileId == 0xbd)
    {
        int n = pWorld->playerY * 10 + pWorld->playerX;
        if (pWorld->mapGrid[n].zoneType == ZONE_TYPE_FIND_USEFUL_DROP)
        {
            if (pWorld->mapGrid[n].flagA == 0)
            {
                int id;
                if (pWorld->mapGrid[n].field30 == 1)
                    id = pWorld->questItemsA.GetAt(pWorld->questItemsA.GetSize() - 1);
                else
                    id = pWorld->questItemsB.GetAt(pWorld->questItemsB.GetSize() - 1);
                CString str = ((Puzzle *)pWorld->puzzles.GetAt(id))->text4;
                pWorld->DrawPlayer();
                DrawGameArea(NULL);
                ShowTextDialog(str, pWorld->cameraX + 16, pWorld->cameraY + 16, 0);
                pWorld->mapGrid[n].flagA = 1;
                int id2;
                if (pWorld->mapGrid[n].field30 == 1)
                    id2 = pWorld->questItemsA.GetAt(0);
                else
                    id2 = pWorld->questItemsB.GetAt(0);
                int nTile = ((Puzzle *)pWorld->puzzles.GetAt(id2))->itemA;
                BlitTile((short)ty, (short)tx, 2, (Tile *)pWorld->tiles.GetAt(nTile));
                DrawGameArea(NULL);
                nPickupX = tx;
                nPickupY = ty;
                nPickupTileId = nTile;
                pPickupObj = NULL;
                nTransitionStep = 0;
                bBlinkState = 0;
                pWorld->nFrameMode = 9;
                bMouseCaptured = 0;
                pWorld->mapGrid[n].flagB = 1;
            }
            else
            {
                int id;
                if (pWorld->mapGrid[n].field30 == 1)
                    id = pWorld->questItemsA.GetAt(pWorld->questItemsA.GetSize() - 1);
                else
                    id = pWorld->questItemsB.GetAt(pWorld->questItemsB.GetSize() - 1);
                CString str = ((Puzzle *)pWorld->puzzles.GetAt(id))->text3;
                pWorld->DrawPlayer();
                DrawGameArea(NULL);
                if (pWorld->gameState == 1)
                    str.LoadString(0xe00b);
                ShowTextDialog(str, pWorld->cameraX + 16, pWorld->cameraY + 16, 0);
                if (bDialogClickDismissMaybe == 0)
                {
                    bTextDialogShown = 1;
                    bInputLocked = 1;
                }
                else
                {
                    bTextDialogShown = 1;
                    bInputLocked = 0;
                }
            }
        }
    }
    else
    {
        int n = pWorld->playerY * 10 + pWorld->playerX;
        short sSlot = pWorld->mapGrid[n].cellQuestSlot0;
        if (sSlot >= 0)
        {
            int id = pWorld->questItemsA.GetAt(sSlot);
            if (pWorld->mapGrid[n].cellQuestSlot6 >= 0
                && (Tile *)pWorld->tiles.GetAt(pWorld->mapGrid[n].cellQuestSlot6) == pWorld->equippedItem)
            {
                int a = tx * 32 + 16;
                int b = ty * 32 + 16;
                if (pWorld->mapGrid[n].flagA == 0)
                {
                    if (pWorld->mapGrid[n].field30 != 1)
                        id = pWorld->questItemsB.GetAt(sSlot);
                    Puzzle *pPuz = (Puzzle *)pWorld->puzzles.GetAt(id);
                    if (pPuz != NULL)
                    {
                        CString str = pPuz->text1;
                        str += " ";
                        int id3;
                        if (pWorld->mapGrid[n].field30 == 1)
                            id3 = pWorld->questItemsA.GetAt(pWorld->mapGrid[n].cellQuestSlot0 + 1);
                        else
                            id3 = pWorld->questItemsB.GetAt(pWorld->mapGrid[n].cellQuestSlot0 + 1);
                        str += ((Puzzle *)pWorld->puzzles.GetAt(id3))->text3;
                        pWorld->DrawPlayer();
                        DrawGameArea(NULL);
                        ShowTextDialog(str, a, b, 0);
                        if (bDialogClickDismissMaybe == 0)
                        {
                            bTextDialogShown = 1;
                            bInputLocked = 1;
                        }
                        else
                        {
                            bTextDialogShown = 1;
                            bInputLocked = 0;
                        }
                    }
                }
                else
                {
                    if (pWorld->mapGrid[n].field30 != 1)
                        id = pWorld->questItemsB.GetAt(sSlot);
                    Puzzle *pPuz = (Puzzle *)pWorld->puzzles.GetAt(id);
                    if (pPuz != NULL)
                    {
                        CString str = pPuz->text2;
                        pWorld->DrawPlayer();
                        DrawGameArea(NULL);
                        ShowTextDialog(str, a, b, 0);
                        if (bDialogClickDismissMaybe == 0)
                        {
                            bTextDialogShown = 1;
                            bInputLocked = 1;
                        }
                        else
                        {
                            bTextDialogShown = 1;
                            bInputLocked = 0;
                        }
                    }
                }
            }
        }
    }
    pWorld->unk3370 = 0;
}

// FUNCTION: YODA 0x0040f490  (??_GCEdit@@UAEPAXI@Z — thin scalar dtor; identity vtable-proven:
//   its dtor target 0x447f8b is the dtor in wndDialogText's funclet (member @0x298, vft 0x44dcd4))
// FUNCTION: YODA 0x0040fc80  (??_GCScrollBar@@UAEPAXI@Z — thin scalar dtor; vft 0x44dda4 is the
//   CScrollBar vtable stored by InvScrollBar's inline base ctor; dtor target 0x447ff9 = ~CScrollBar)

// FUNCTION: YODA 0x0040fca0
// GameView::ClassifyTile — classifies the object at (x,y) for the Artoo
// hint table in OnDragItem (result 0..0x14 -> string ids 0xe020..0xe038;
// -1 = no hint -> rotating small talk). Win/lose state first, then the
// one-shot beep flags, the player cell (0xe), a layer-1 tile-id switch
// (pushable 7 via TILE flag, doors 6/8, x-wing 0x12, ...), the enemy/NPC
// flag bits (0/5), a friendly-tile switch (2) and the valid-hint-target
// switch (4).
// PHASE-DISPLACED (v24): byte-EXACT under the v23 dial (1569B, first compile);
// the OnDragItem+SoundInit additions rotated it to align=240 (pure reg/schedule
// tie-breaks). Source proven correct — G1.
int CDeskcppView::ClassifyTile(int x, int y)
{
    if (pWorld->gameState == 1)
        return 9;
    if (pWorld->gameState == -1)
        return 10;
    if (bArtooBeepPending0Maybe != 0)
    {
        bArtooBeepPending0Maybe = 0;
        return 0x13;
    }
    if (bDropOnArtooMaybe != 0)
    {
        bDropOnArtooMaybe = 0;
        return 0x13;
    }
    if (bDraggedArtooBlockedMaybe != 0)
    {
        bDraggedArtooBlockedMaybe = 0;
        return 0x14;
    }
    if (bDropOutsideViewMaybe != 0)
    {
        bDropOutsideViewMaybe = 0;
        return 0x13;
    }
    if (pWorld->cameraX / 32 == x && pWorld->cameraY / 32 == y)
        return 0xe;
    int t = (short)pWorld->currentZone->GetTile(x, y, 1);
    if (t < 0)
    {
        int t0 = (short)pWorld->currentZone->GetTile(x, y, 0);
        if (t0 == 0x218)
            return 0x10;
        return t0 == 0x219 ? 0xf : -1;
    }
    unsigned int flags = ((Tile *)pWorld->tiles.GetAt(t))->flags;
    if ((flags & 8) != 0)
        return 7;
    if (t <= 0 || t == 0x7f2)
        return -1;
    switch (t)
    {
    case 0x30c:
        return 6;
    case 0x200:
    case 0x201:
    case 0x202:
        return 0x12;
    case 0x314:
    case 0x77c:
    case 0x77d:
    case 0x77e:
        return 1;
    case 0x310:
    case 0x58a:
    case 0x6e3:
    case 0x6e4:
        return 0xc;
    case 0x3b8:
    case 0x3b9:
    case 0x3ba:
    case 0x3bb:
        return 8;
    case 0x31d:
        return 0x11;
    case 0x645:
    case 0x646:
    case 0x64f:
    case 0x650:
        return 0xb;
    case 0x6e8:
    case 0x6ee:
    case 0x6ef:
    case 0x6f0:
    case 0x6f1:
        return 0xd;
    }
    if ((flags & 0x20000) != 0)
        return 0;
    if ((flags & 0x40000) != 0)
        return 5;
    switch (t)
    {
    case 0x10:
    case 0x102:
    case 0x279:
    case 0x27b:
    case 0x27c:
    case 0x48d:
    case 0x6e5:
        return 2;
    }
    switch (t)
    {
    case 0x46:
    case 0x47:
    case 0x48:
    case 0x49:
    case 0x4a:
    case 0x4b:
    case 0x4c:
    case 0x91:
    case 0x95:
    case 0x98:
    case 0x99:
    case 0xdc:
    case 0xdd:
    case 0xdf:
    case 0xe7:
    case 0xe8:
    case 0xe9:
    case 0x15e:
    case 0x246:
    case 0x248:
    case 0x24a:
    case 0x24c:
    case 0x2be:
    case 0x2c5:
    case 0x2f3:
    case 0x2f4:
    case 0x2f7:
    case 0x2f8:
    case 0x324:
    case 0x326:
    case 0x3d7:
    case 0x3d8:
    case 0x417:
    case 0x418:
    case 0x439:
    case 0x458:
    case 0x460:
    case 0x4eb:
    case 0x5b5:
    case 0x5b6:
    case 0x5c0:
    case 0x5c1:
    case 0x600:
    case 0x603:
    case 0x608:
        return 4;
    }
    return -1;
}

// FUNCTION: YODA 0x004102d0
// GameView::OnDragItem — drop handler for the dragged inventory item released at
// pixel (x, y) (from OnLButtonUp). Out-of-range drops (not within one cell of the
// player, and not the detonator 0x202 / Artoo 0x31a) just buzz. Dropping ON the
// player uses the item: a weapon re-equips the matching character and reloads its
// charge from the per-weapon ammo store (Force/saber default 30; 0x1ff blaster 15,
// 0x200 30, 0x201 10, 0x204/0x205 15); a health pack heals by tile id (100/50/25);
// a harmful item (flag 1<<21) costs 25 health. The detonator runs the 5-phase
// palette effect (100-tick clock() busy-waits); dragging Artoo (0x31a) shows a help
// balloon keyed by ClassifyTile (LoadString 0xe020..0xe038 + 5 rotating "can't
// help" lines). Anything else resolves the world-map cell's puzzle (TRANSACTION
// trade at an NPC / TRADE key on a lock: stamps flagA/flagB, queues the reward via
// nPickup*, gameState 9) and fires the DragItem IACT (event 3).
// sic: the Artoo cases 0x13/0x14 return WITHOUT restoring the palette or releasing
// the DC (leak — see docs/engine-bugs.md #14); the nPuzIdx < 0 guard is dead (WORD
// zero-extend, bug #10 family); pChar->unk48 self-assigns when the dragged weapon
// is already equipped.
//
// EFFECTIVE MATCH-WIP (945/924 insns; align dominated by one GLOBAL register-role
// rotation: orig px=EDI py=ESI + nWorldX/Y both slot-homed, ours px=EBX py=EDI +
// nWorldY riding ESI into the IACT arm — the ShowWinMessage/OnTimer global-rank
// family). Minimal-TU probe scored IDENTICAL solo (1046 vs 1054) ⇒ header-dial,
// not TU-position — G1. Structure proven: every arm, call, constant, switch and
// branch shape aligns. Cracks that landed: refill arms are &field POINTER LOCALS
// (`short *p = &pWorld->weaponState[i]; short a = *p; if (a <= 0) { *p = K;
// a = pWorld->weaponState[i]; }` — reproduces the add-reg,0xNNNN store form);
// quest-list selector spelled `if (field30 != 1) B; else A;` (then-jump polarity);
// the 2-case nType dispatch is a SWITCH (compares up front, je/je/jmp);
// `int t = (short)GetTile(...)` (movsx, int compare vs cellQuestSlot6);
// eager `int nQuestIdx` widening before the selector branch; the reward scan
// DECREMENTS nObjs itself (`nObjs--` — a separate `int n = nObjs` leaves a
// self-move fingerprint); characters walk = GetData() hoisted pointer while
// tiles/objects use GetAt (per-iteration m_pData reloads). Parked residuals
// (probed, rotation-coupled): the PlaySound(6)+DrawText(0) tails — orig
// cross-jumps full-health→final-else (mov ecx,edx; jmp into the call) and
// weapon→DrawText, ours emits 3 full copies (cl merged the heal/poison arms
// into the IACT PlaySound tail instead; merge-partner choice not steerable);
// ladder value-arm cluster flushes at our then-block end vs orig's outer join
// with ONE shared 0x32 arm (`||`-inverted spelling canonicalizes back, lesson
// #6); ours materializes EBX=0 in the trade arm where orig uses immediates;
// nObjs slot-homed vs orig ESI; 0x12/0x1fe arms load/TEST/store vs ours
// load/store/test (1-insn scheduling drift); i+8 hoist + bFound=1 placement
// (imm-store-batching family); detonator clock-wait c/end reg swap; sx/sy
// SI/DI swap.
void CDeskcppView::OnDragItem(int x, int y, Tile *pTile)
{
    CDC *pDC = GetDC();
    CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
    int nWorldX = pWorld->playerX;
    int nWorldY = pWorld->playerY;
    pWorld->equippedItem = pTile;
    int tx = x / 32;
    int ty = y / 32;
    nDetonatorX = tx;
    nDetonatorY = ty;
    int px = pWorld->cameraX / 32;
    int nSound = 6;
    int py = pWorld->cameraY / 32;
    pWorld->DrawPlayer();
    DrawGameArea(0);
    int id = pWorld->FindTile(pTile);
    if (id != 0x202 && id != 0x31a
        && (px - 1 > tx || px + 1 < tx || py - 1 > ty || py + 1 < ty))
    {
        PlaySound(6);
    }
    else if (px == tx && ty == py && id != 0x202 && id != 0x31a)
    {
        if (pTile->flags & TILE_WEAPON)
        {
            int nCount = pWorld->characters.GetSize();
            int bFound = 0;
            int i = 0;
            if (nCount > 0)
            {
                Character **paChars = (Character **)pWorld->characters.GetData();
                do
                {
                    Character *pChar = *paChars;
                    int nCharTile;
                    if (pChar != 0 && (nCharTile = pChar->frames[7]) >= 0
                        && (Tile *)pWorld->tiles.GetAt(nCharTile) == draggedTile)
                    {
                        if (pChar == pWorld->currentWeapon)
                        {
                            pChar->unk48 = pWorld->currentWeapon->unk48;
                        }
                        else switch (nCharTile)
                        {
                        case 0x12:
                            if ((pChar->unk48 = pWorld->ammoLightsaber) == 0)
                                pChar->unk48 = 30;
                            PlaySound(0x1f);
                            break;
                        case 0x1fe:
                            if ((pChar->unk48 = pWorld->ammoTheForce) == 0)
                                pChar->unk48 = 30;
                            PlaySound(0x1f);
                            break;
                        case 0x1ff:
                            if (pWorld->weaponState[0] > 0)
                            {
                                pChar->unk48 = pWorld->weaponState[0];
                            }
                            else
                            {
                                pChar->unk48 = 15;
                                pWorld->weaponState[0] = 15;
                            }
                            PlaySound(0x34);
                            break;
                        case 0x200:
                        {
                            short *p = &pWorld->weaponState[1];
                            short a = *p;
                            if (a <= 0)
                            {
                                *p = 30;
                                a = pWorld->weaponState[1];
                            }
                            pChar->unk48 = a;
                            PlaySound(0x20);
                            break;
                        }
                        case 0x201:
                        {
                            short *p = &pWorld->weaponState[2];
                            short a = *p;
                            if (a <= 0)
                            {
                                *p = 10;
                                a = pWorld->weaponState[2];
                            }
                            pChar->unk48 = a;
                            PlaySound(0x20);
                            break;
                        }
                        case 0x204:
                        {
                            short *p = &pWorld->weaponState[3];
                            short a = *p;
                            if (a <= 0)
                            {
                                *p = 15;
                                a = pWorld->weaponState[3];
                            }
                            pChar->unk48 = a;
                            break;
                        }
                        case 0x205:
                        {
                            short *p = &pWorld->nCurrentAmmoMaybe;
                            short a = *p;
                            if (a <= 0)
                            {
                                *p = 15;
                                a = pWorld->nCurrentAmmoMaybe;
                            }
                            pChar->unk48 = a;
                            break;
                        }
                        }
                        int nSlot = i + 8;
                        bFound = 1;
                        pWorld->currentWeapon = pChar;
                        pWorld->unk2e30 = nSlot;
                        draggedTile = 0;
                        break;
                    }
                    paChars++;
                    i++;
                } while (i < nCount);
            }
            if (bFound == 0)
                pWorld->currentWeapon = 0;
            DrawWeaponBox(0);
            DrawWeaponIcon(0);
            DrawText(0);
        }
        else if (pTile->flags & TILE_HEALTH_PACK)
        {
            if (pWorld->healthHi == 1 && pWorld->healthLo == 1)
            {
                PlaySound(6);
                DrawText(0);
            }
            else
            {
                int nHeal = pWorld->FindTile(pTile);
                if (nHeal <= 0x1fa)
                {
                    if (nHeal < 0x1f9)
                    {
                        if (nHeal == 0x1e0 || nHeal == 0x1e2)
                            nHeal = 50;
                        else
                            nHeal = 0;
                    }
                    else
                    {
                        nHeal = 100;
                    }
                }
                else if (nHeal != 0x1fb)
                {
                    if (nHeal < 0x4ac || nHeal > 0x4ae)
                        nHeal = 0;
                    else
                        nHeal = 50;
                }
                else
                {
                    nHeal = 25;
                }
                AddHealth(nHeal);
                RemoveItem(pTile);
                PlaySound(0);
            }
        }
        else if (pTile->flags & TILE_ITEM_HARMFUL_MAYBE)
        {
            AddHealth(-25);
            RemoveItem(pTile);
            PlaySound(4);
        }
        else
        {
            PlaySound(6);
            DrawText(0);
        }
    }
    else if (px == tx && ty == py && id == 0x202)
    {
        PlaySound(6);
    }
    else if (id == 0x202 && bInvClickPending == 0)
    {
        RemoveItem(pTile);
        PlaySound(0xb);
        while (nDetonatorPhase < 5)
        {
            CyclePalette();
            StepDetonatorEffect();
            nDetonatorPhase++;
            long c = clock();
            long end = c + 100;
            while (c < end)
                c = clock();
        }
        nDetonatorPhase = 0;
        pDC->SelectPalette(pOldPal, 0);
        ReleaseDC(pDC);
        return;
    }
    else if (id == 0x31a && bInvClickPending == 0)
    {
        CString str;
        switch (ClassifyTile(tx, ty))
        {
        case -1:
            // Artoo starts yapping about random things when dragged onto
            // something he can't help with — 5 rotating lines.
            switch (artooAnyhowHelpIdx)
            {
            case 0:
                str.LoadString(0xe027);
                break;
            case 1:
                str.LoadString(0xe028);
                break;
            case 2:
                str.LoadString(0xe029);
                break;
            case 3:
                str.LoadString(0xe02a);
                break;
            case 4:
                str.LoadString(0xe02b);
                break;
            }
            artooAnyhowHelpIdx = artooAnyhowHelpIdx + 1;
            if (artooAnyhowHelpIdx > 4)
                artooAnyhowHelpIdx = 0;
            break;
        case 0:
            str.LoadString(0xe022);
            break;
        case 1:
            str.LoadString(0xe02c);
            break;
        case 2:
            str.LoadString(0xe020);
            break;
        case 4:
            str.LoadString(0xe023);
            break;
        case 5:
            str.LoadString(0xe025);
            break;
        case 6:
            str.LoadString(0xe026);
            break;
        case 7:
            str.LoadString(0xe024);
            break;
        case 8:
            str.LoadString(0xe021);
            break;
        case 9:
            str.LoadString(0xe02d);
            break;
        case 10:
            str.LoadString(0xe02e);
            break;
        case 0xb:
            str.LoadString(0xe02f);
            break;
        case 0xc:
            str.LoadString(0xe030);
            break;
        case 0xd:
            str.LoadString(0xe031);
            break;
        case 0xe:
            str.LoadString(0xe033);
            break;
        case 0xf:
            str.LoadString(0xe034);
            break;
        case 0x10:
            str.LoadString(0xe035);
            break;
        case 0x11:
            str.LoadString(0xe036);
            break;
        case 0x12:
            str.LoadString(0xe038);
            break;
        case 0x13:
            PlaySound(6);
            YODA_SIC_RETURN(BUGLOG(("sic#14 OnDragItem: Artoo case 0x13 — DC+palette released\n")); pDC->SelectPalette(pOldPal, 0); ReleaseDC(pDC);) // sic: leaks the DC + selected palette
        case 0x14:
            PlaySound(6);
            YODA_SIC_RETURN(BUGLOG(("sic#14 OnDragItem: Artoo case 0x14 — DC+palette released\n")); pDC->SelectPalette(pOldPal, 0); ReleaseDC(pDC);) // sic: leaks the DC + selected palette
        }
        // Show the Artoo speech balloon over the target cell.
        pWorld->GetTileData(0x31a);
        short sx = (short)tx;
        short sy = (short)ty;
        BlitTile(sy, sx, 0, pTile);
        DrawGameArea(0);
        ShowTextDialog(str, tx * 32 + 16, ty * 32 + 16, 0);
        DrawZoneCell(sx, sy);
        DrawGameArea(0);
        pDC->SelectPalette(pOldPal, 0);
        ReleaseDC(pDC);
        return;
    }
    else
    {
        if (id == 0x1ff)
        {
            PlaySound(6);
        }
        else
        {
            int nCell = nWorldX + nWorldY * 10;
            if (pWorld->mapGrid[nCell].cellQuestSlot0 >= 0
                && pWorld->mapGrid[nCell].flagA == 0)
            {
                int nQuestIdx = pWorld->mapGrid[nCell].cellQuestSlot0;
                unsigned short *paList;
                if (pWorld->mapGrid[nCell].field30 != 1)
                    paList = pWorld->questItemsB.GetData();
                else
                    paList = pWorld->questItemsA.GetData();
                int nPuzIdx = paList[nQuestIdx];
                if (nPuzIdx >= 0)   // sic: dead guard — WORD zero-extends
                {
                    Puzzle *pPuz = (Puzzle *)pWorld->puzzles.GetAt(nPuzIdx);
                    if (pPuz != 0
                        && (Tile *)pWorld->tiles.GetAt(pWorld->mapGrid[nCell].cellItemA) == pTile)
                    {
                        switch (pPuz->nType)
                        {
                        case PUZZLE_TYPE_TRANSACTION:
                        {
                            int t = (short)pWorld->currentZone->GetTile(tx, ty, 1);
                            ZoneObj *pObj = pWorld->currentZone->FindObjectAt(tx, ty);
                            if (pWorld->mapGrid[nCell].cellQuestSlot6 == t
                                && pObj != 0 && pObj->state == 1)
                            {
                                nSound = 0;
                                int nReward = pWorld->mapGrid[nCell].cellItemC;
                                if (nReward >= 0)
                                {
                                    RemoveItem(pTile);
                                    DrawText(0);
                                    pWorld->mapGrid[nCell].flagA = 1;
                                    CString strReply = pPuz->text2;
                                    ShowTextDialog(strReply, tx * 32 + 16, ty * 32 + 16, 0);
                                    nPickupX = tx;
                                    nPickupY = ty;
                                    nPickupTileId = nReward;
                                    pPickupObj = 0;
                                    nTransitionStep = 0;
                                    bBlinkState = 0;
                                    pWorld->mapGrid[nCell].flagB = 1;
                                    bMouseCaptured = 0;
                                    pWorld->nFrameMode = 9;
                                }
                            }
                            else
                            {
                                nSound = 6;
                            }
                            break;
                        }
                        case PUZZLE_TYPE_TRADE:
                        {
                            int i = 0;
                            int nObjs = pWorld->currentZone->objects.GetSize();
                            int bFound = 0;
                            if (nObjs > 0)
                            {
                                do
                                {
                                    if (bFound)
                                        break;
                                    CDeskcppDoc *pW = pWorld;
                                    ZoneObj *pObj = (ZoneObj *)pW->currentZone->objects.GetAt(i);
                                    if (pObj->x == tx && pObj->y == ty
                                        && pObj->state != 0 && pObj->type == OBJ_LOCK)
                                    {
                                        bFound++;
                                        pW->mapGrid[nCell].flagA = 1;
                                    }
                                    i++;
                                } while (i < nObjs);
                            }
                            if (bFound)
                            {
                                nSound = 0;
                                if (!(pTile->flags & TILE_KEYCARD))
                                {
                                    RemoveItem(pTile);
                                    DrawText(0);
                                }
                                int nReward = pWorld->mapGrid[nCell].cellItemC;
                                if (nReward >= 0 && nObjs > 0)
                                {
                                    int j = 0;
                                    do
                                    {
                                        CDeskcppDoc *pW = pWorld;
                                        ZoneObj *pObj = (ZoneObj *)pW->currentZone->objects.GetAt(j);
                                        if (pObj->x == tx && pObj->y == ty
                                            && pObj->state != 0
                                            && pObj->type == OBJ_QUEST_ITEM_SPOT
                                            && pObj->arg == nReward)
                                        {
                                            pW->mapGrid[nCell].flagB = 1;
                                            nPickupX = tx;
                                            nPickupTileId = nReward;
                                            pPickupObj = 0;
                                            nPickupY = ty;
                                            nTransitionStep = 0;
                                            bBlinkState = 0;
                                            bMouseCaptured = 0;
                                            pWorld->nFrameMode = 9;
                                        }
                                        j++;
                                        nObjs--;
                                    } while (nObjs != 0);
                                }
                            }
                            else
                            {
                                nSound = 6;
                            }
                            break;
                        }
                        }
                    }
                }
            }
            else
            {
                ZoneObj *pObj;
                if (pWorld->mapGrid[nCell].cellItemA >= 0
                    && (Tile *)pWorld->tiles.GetAt(pWorld->mapGrid[nCell].cellItemA) == pTile
                    && (pObj = pWorld->currentZone->FindObjectAt(tx, ty)) != 0
                    && pObj->state == 1 && pObj->type == OBJ_LOCK)
                {
                    nSound = 0;
                    pWorld->mapGrid[nCell].flagA = 1;
                    if (!(pTile->flags & TILE_KEYCARD))
                    {
                        RemoveItem(pTile);
                        DrawText(0);
                    }
                }
            }
            int nPrevMode = pWorld->nFrameMode;
            pWorld->nFrameMode = 1;
            unsigned int nMask = pWorld->currentZone->IactRun(3, tx, ty, 0, 0, 0,
                                                              pDC, pWorld, this);
            int nZT = pWorld->currentZone->type;
            if (nZT == 6 || nZT == 7 || nZT == 0xb)
                TriggerHotspotsMaybe();
            if (nPrevMode == 4)
            {
                if (pWorld->nFrameMode != 9 && pWorld->nFrameMode != 6)
                    pWorld->nFrameMode = 3;
            }
            else if (pWorld->nFrameMode != 6 && pWorld->nFrameMode != 9)
            {
                pWorld->nFrameMode = nPrevMode;
            }
            if (!(nMask & 1))
                PlaySound(nSound);
        }
    }
    pDC->SelectPalette(pOldPal, 0);
    ReleaseDC(pDC);
}

// FUNCTION: YODA 0x00411010  (??_GCBitmapButton@@UAEPAXI@Z — TU COMDAT copy; the GameView
//   ctor's three CBitmapButton members odr-use it. Emitted mid-TU after OnDragItem.)
// FUNCTION: YODA 0x004110d0  (??1CBitmapButton@@UAE@XZ — the plain dtor; GameView's ctor/dtor
//   EH funclets for btnDialogClose/btnDialogUp/btnDialogDown jmp here.)

// FUNCTION: YODA 0x00411180
// GameView::ScrollZoneTransition — edge-of-zone walk transition: scrolls
// the 288px viewport 16px per step in the scrollDirX/Y direction, blitting
// the on-screen area over itself and painting the newly exposed strip from
// the world canvas, with a 50ms clock() busy-wait per step. Runs the
// enter-zone IACT (event 4) first with the player hidden, and stamps the
// map cell solved + mode 3 when done. One-shot: seeds the MIDILoad
// registry default on first use (bMidiProfileInitMaybe).
// EFFECTIVE-WIP (255/264 insns — ours 9 SHORTER): every call, arm and
// constant aligned; the residual is ONE register-budget decision echoed
// TU-wide: the orig spills THIS to [esp] and n2 to [esp+0x14] (reloading
// this before nearly every statement — the OnTimer this-reload family),
// assigning the four callee-saved regs to pDC(ESI)/n(EDI)/per-arm scratch
// (EBX/EBP incl. the GetSafeHdc temp); ours keeps this=ESI, pDC=EDI,
// n2=EBX, n=EBP and reads fields directly. n/n2 decl order inert.
// Cracks: int *pHide = &bHidePlayer pointer-local (add eax,0x2e54
// store form), CWinApp *pApp = AfxGetApp() local (single ModuleState
// call), GetSafeHdc() for the BitBlt src hdc, one test-eax three-way
// dispatch (dirX >0 / <0 / else dirY), clock()+50 busy-wait, per-arm
// duplicated Canvas::BitBlt calls cross-jumping into one tail.
void CDeskcppView::ScrollZoneTransition()
{
    CDC *pDC = GetDC();
    if (bMidiProfileInitMaybe == 0)
    {
        CWinApp *pApp = AfxGetApp();
        if (pApp->GetProfileInt("OPTIONS", "MIDILoad", -1) == -1)
            pApp->WriteProfileInt("OPTIONS", "MIDILoad", 1);
        bMidiProfileInitMaybe = 1;
    }
    CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
    int *pHide = &pWorld->bHidePlayer;
    int nOldHide = *pHide;
    *pHide = 1;
    unsigned int nMask = pWorld->currentZone->IactRun(4, -1, -1, -1, -1, -1, NULL, pWorld, this);
    if (pWorld->currentZone->activatedFlag == 0)
        DrawEntities();
    if ((nMask & 0x20) != 0)
        DrawWholeZone();
    int n = 0;
    int n2 = 0x10;
    do
    {
        if (pWorld->scrollDirX > 0)
        {
            ::BitBlt(pDC->m_hDC, pWorld->rectUnk3274.left, pWorld->rectUnk3274.top,
                     0x120 - n2, 0x120, pDC->GetSafeHdc(),
                     pWorld->rectUnk3274.left + 0x10, pWorld->rectUnk3274.top, SRCCOPY);
            pWorld->pCanvas->BitBlt(pDC, pWorld->rectUnk3274.left - n2 + 0x120,
                                    pWorld->rectUnk3274.top, n2, 0x120, 0, pWorld->nViewTop);
        }
        else if (pWorld->scrollDirX < 0)
        {
            int e = pWorld->rectUnk3274.left + n;
            ::BitBlt(pDC->m_hDC, e + 0x10, pWorld->rectUnk3274.top,
                     0x110 - n, 0x120, pDC->GetSafeHdc(),
                     e, pWorld->rectUnk3274.top, SRCCOPY);
            pWorld->pCanvas->BitBlt(pDC, pWorld->rectUnk3274.left, pWorld->rectUnk3274.top,
                                    n + 0x10, 0x120, 0x22f - n, pWorld->nViewTop);
        }
        else if (pWorld->scrollDirY < 0)
        {
            int e = pWorld->rectUnk3274.top + n;
            ::BitBlt(pDC->m_hDC, pWorld->rectUnk3274.left, e + 0x10,
                     0x120, 0x110 - n, pDC->GetSafeHdc(),
                     pWorld->rectUnk3274.left, e, SRCCOPY);
            pWorld->pCanvas->BitBlt(pDC, pWorld->rectUnk3274.left, pWorld->rectUnk3274.top,
                                    0x120, n + 0x10, pWorld->nViewLeft, 0x22f - n);
        }
        else if (pWorld->scrollDirY > 0)
        {
            ::BitBlt(pDC->m_hDC, pWorld->rectUnk3274.left, pWorld->rectUnk3274.top,
                     0x120, 0x120 - n2, pDC->GetSafeHdc(),
                     pWorld->rectUnk3274.left, pWorld->rectUnk3274.top + 0x10, SRCCOPY);
            pWorld->pCanvas->BitBlt(pDC, pWorld->rectUnk3274.left,
                                    pWorld->rectUnk3274.top - n2 + 0x120, 0x120, n2,
                                    pWorld->nViewLeft, 0);
        }
        long c = clock();
        long end = c + 50;
        while (c < end)
            c = clock();
        n2 += 0x10;
        n += 0x10;
    } while (n2 <= 0x120);
    pWorld->bHidePlayer = nOldHide;
    pWorld->currentZone->activatedFlag = 1;
    DrawWholeZone();
    pWorld->mapGrid[pWorld->playerY * 10 + pWorld->playerX].flagSolved = 1;
    DrawDirectionArrows(pDC);
    pDC->SelectPalette(pOldPal, 0);
    ReleaseDC(pDC);
    pWorld->nFrameMode = 3;
}

// FUNCTION: YODA 0x00411520
// GameView::SoundInit — one-shot WAVMIX32 session setup. Opens the mixer, loads
// every named sound (World.soundNames[64], "sfx\<name>") into g_waveHandles,
// opens channel 8 and activates; on any failure frees the loaded waves, closes
// the session and turns sound+music off. On success spawns the music pump
// thread (MusicThreadProcMaybe) with its tick event.
void CDeskcppView::SoundInit()
{
    if (soundSession != 0)
        return;
#ifdef GAME_INDY
    // MIDI music (MCI) is independent of the WaveMix SFX session — DESKADV opens the
    // sequencers even when WaveMix fails (FUN_1018_4c54 tail runs on the failure paths).
    Indy_MidiOpenAll(pWorld);
#endif
    soundSession = WaveMixInit();
    if (soundSession == 0)
    {
        pWorld->nSoundEnabled = 0;
        pWorld->nMusicEnabled = 0;
        return;
    }
    char szPath[100];
    char szName[100];
    int i = 0;
    do
    {
        if (pWorld->soundNames[i].GetLength() > 0)
        {
#ifdef GAME_INDY
            // .MIDs were opened as MCI sequencers above (Indy_MidiOpenAll); WaveMix only WAVs.
            if (strstr(pWorld->soundNames[i], ".MID") != NULL)
            {
                i++;
                continue;
            }
            // Indy ships its WAVs in the game directory, not Yoda's `sfx\` subfolder — so load by
            // bare name (WaveMixOpenWave resolves relative to cwd). Prefixing `sfx\` silently fails
            // to open every wave -> no sound. Yoda #else = exact original.
            szPath[0] = 0;
#else
            strcpy(szPath, "sfx\\");
#endif
            strcpy(szName, pWorld->soundNames[i]);
            strcat(szPath, szName);
            g_waveHandles[i] = WaveMixOpenWave(soundSession, szPath, 0, 1);
        }
        i++;
    } while (i < 64);
#ifdef GAME_INDY
    {
        // eep.wav: Indy's hardcoded 15th wave outside SNDS (DESKADV FUN_1018_4c54, the
        // string @1018:9718), played as sound id SND_INDY_EEP. Twin of Yoda's id-0x25 quirk.
        char szEep[12];
        strcpy(szEep, "eep.wav");
        g_waveHandles[SND_INDY_EEP] = WaveMixOpenWave(soundSession, szEep, 0, 1);
    }
#endif
    if (WaveMixOpenChannel(soundSession, 8, 1) != 0)
    {
        int *p = g_waveHandles;
        do
        {
            if (*p != 0)
                WaveMixFreeWave(soundSession, *p);
            p++;
        } while (p < g_waveHandles + 64);
        WaveMixCloseSession(soundSession);
        soundSession = 0;
        pWorld->nSoundEnabled = 0;
        pWorld->nMusicEnabled = 0;
    }
    else if (WaveMixActivate(soundSession, 1) != 0)
    {
        pWorld->nSoundEnabled = 0;
        WaveMixCloseChannel(soundSession, 8, 1);
        int *p = g_waveHandles;
        do
        {
            if (*p != 0)
                WaveMixFreeWave(soundSession, *p);
            p++;
        } while (p < g_waveHandles + 64);
        WaveMixCloseSession(soundSession);
        soundSession = 0;
        pWorld->nSoundEnabled = 0;
        pWorld->nMusicEnabled = 0;
    }
    if (soundSession != 0)
    {
        g_hWaveMixEvent = CreateEvent(0, 1, 0, 0);
        pMusicThread = AfxBeginThread(MusicThreadProcMaybe, 0, 0, 0, 0, 0);
    }
    else
    {
        pMusicThread = 0;
    }
}

// FUNCTION: YODA 0x00411730
// GameView::OnLButtonDown — WM_LBUTTONDOWN by frame mode. Mode 2: back to 3.
// Mode 3: scrollbar click = nothing, inventory rect = bInvClickPending, viewport
// (inflated 12px) = set the walk target (fine coords *32), SetCapture, phase++,
// mode 2. Mode 4: inventory rect only. Mode 7 reason 2 (win/lose screen): a
// click past step 50 returns to the pending zone. Mode 7 reason 4 (world map):
// clicking a visited cell shows its zone-name/status balloon (per zone type),
// an ENEMY_TERRITORY cell teleports there when the locator is in inventory
// slot 0 and the zone has an active teleporter; anything else buzzes and closes
// the map back to pMapReturnZone (four copy-pasted close blocks with genuine
// statement-order drift — transcribed verbatim). Mode 9: click on the pickup
// cell sets bPickupClickPendingMaybe.
// EFFECTIVE MATCH (70/2845 bytes differ; 761/750 insns). Residuals: (a) the
// mode-3 walk-target pair — orig interleaves the X and Y chains (Y loads first,
// X stores first, perfectly paired); plain statements emit sequentially and
// int-local spellings are WORSE (+36 insns) — the TZD arg-eval-position family;
// (b) ONE global EBX↔EDI role swap (gy and the pCell base) echoing through the
// map-click case — allocator tie-break, G1; (c) jump/byte-table tail noise.
// Cracks that landed: balloonY is a value-ternary with the *28 DUPLICATED in
// both arms ((gy == 0) ? gy * 28 : (gy + 1) * 28 — polarity load-bearing); the
// CString balloon arms live in INNER SCOPES so the dtor runs BEFORE
// bMouseCaptured = 0 (dtor-vs-store order is source-visible); the four buzz-
// close blocks are verbatim copies with per-copy statement order.
void CDeskcppView::OnLButtonDown(UINT nFlags, CPoint point)
{
    bInvClickPending = 0;
    if (bInputLocked != 0)
        return;
    if (nFlags & 4)
        bShiftHeld = 1;
    else
        bShiftHeld = 0;
    switch (pWorld->nFrameMode)
    {
    case 2:
        pWorld->nFrameMode = 3;
        break;
    case 3:
    {
        RECT rc;
        rc.left = pWorld->rectUnk3274.left - 12;
        rc.right = pWorld->rectUnk3274.right + 12;
        rc.top = pWorld->rectUnk3274.top - 12;
        rc.bottom = pWorld->rectUnk3274.bottom + 12;
        if (PtInRect(&pWorld->rectInvScroll, point) == 0)
        {
            if (PtInRect(&pWorld->rectUnk3284, point) != 0)
            {
                bInvClickPending = 1;
            }
            else if (PtInRect(&rc, point) != 0 && bInputLocked == 0)
            {
                pWorld->nWalkTargetX =
                    (pWorld->nViewLeft - pWorld->rectUnk3274.left + point.x) * 32;
                pWorld->nWalkTargetY =
                    (pWorld->nViewTop - pWorld->rectUnk3274.top + point.y) * 32;
                SetCapture();
                nWalkFramePhase++;
                bMouseCaptured = 1;
                pWorld->nFrameMode = 2;
            }
        }
        break;
    }
    case 4:
        if (PtInRect(&pWorld->rectUnk3284, point) != 0)
            bInvClickPending = 1;
        break;
    case 7:
        switch (pWorld->nMapChangeReason)
        {
        case 2:
            if (nTransitionStep > 0x32
                && pWorld->GetZoneIndex(pWorld->pPendingZone) != 0x152)
            {
                pWorld->currentZone = pWorld->pPendingZone;
                pWorld->cameraX = pWorld->nextCameraX;
                pWorld->cameraY = pWorld->nextCameraY;
                pWorld->RefreshZone();
                pWorld->UpdateCamera();
                pWorld->bHidePlayer = 0;
                pWorld->DrawPlayer();
                pWorld->nFrameMode = 3;
                DrawGameArea(0);
                DrawDirectionArrows(0);
            }
            break;
        case 4:
        {
            int gx = (point.x + 12) / 28 - 1;
            int gy = (point.y + 12) / 28 - 1;
            int nBalloonX = gx * 28 + 18;
            int nBalloonY = (gy == 0) ? gy * 28 : (gy + 1) * 28;
            if (gx < 0 || gy < 0 || gx > 9 || gy > 9)
            {
                PlaySound(0x23);
                pWorld->currentZone = pMapReturnZone;
                bMapViewOpen = 0;
                pWorld->RefreshZone();
                pWorld->UpdateCamera();
                pWorld->bHidePlayer = 0;
                pWorld->nFrameMode = 3;
                DrawGameArea(0);
                bMouseCaptured = 0;
                bMapTeleportEnabled = 0;
                break;
            }
            if (pWorld->mapGrid[gy * 10 + gx].flagSolved == 0)
            {
                PlaySound(0x23);
                pWorld->currentZone = pMapReturnZone;
                bMapViewOpen = 0;
                bMouseCaptured = 0;
                pWorld->RefreshZone();
                pWorld->UpdateCamera();
                pWorld->bHidePlayer = 0;
                pWorld->nFrameMode = 3;
                DrawGameArea(0);
                pWorld->DrawPlayer();
                break;
            }
            switch (pWorld->mapGrid[gy * 10 + gx].zoneType)
            {
            default:
                PlaySound(0x23);
                pWorld->currentZone = pMapReturnZone;
                bMapViewOpen = 0;
                pWorld->RefreshZone();
                pWorld->bHidePlayer = 0;
                pWorld->UpdateCamera();
                pWorld->nFrameMode = 3;
                DrawGameArea(0);
                bMouseCaptured = 0;
                bMapTeleportEnabled = 0;
                break;
            case ZONE_TYPE_ENEMY_TERRITORY:
                if (bMapTeleportEnabled == 1
                    && ((InvItem *)pWorld->inventory.GetAt(0))->pTile
                       == (Tile *)pWorld->tiles.GetAt(IDX_LOCATOR_ITEM))
                {
                    Zone *pZone = (Zone *)pWorld->zones.GetAt(
                        pWorld->mapGrid[gy * 10 + gx].id);
                    int nObjs = pZone->objects.GetSize();
                    if (nObjs > 0)
                    {
                        int i = 0;
                        ZoneObj **paObjs = (ZoneObj **)pZone->objects.GetData();
                        do
                        {
                            ZoneObj *pObj = *paObjs;
                            if (pObj->type == OBJ_TELEPORTER && pObj->state == 1)
                            {
                                PlaySound(0);
                                pWorld->cameraX = pObj->x << 5;
                                pWorld->cameraY = pObj->y << 5;
                                pWorld->currentZone = pZone;
                                pWorld->nFrameMode = 3;
                                bMapViewOpen = 0;
                                pWorld->bHidePlayer = 0;
                                pWorld->RefreshZone();
                                pWorld->UpdateCamera();
                                DrawGameArea(0);
                                pWorld->playerX = gx;
                                pWorld->playerY = gy;
                                DrawDirectionArrows(0);
                                bMapTeleportEnabled = 0;
                                bMouseCaptured = 0;
                                return;
                            }
                            paObjs++;
                            i++;
                        } while (i < nObjs);
                    }
                }
                PlaySound(0x23);
                pWorld->currentZone = pMapReturnZone;
                bMapViewOpen = 0;
                pWorld->RefreshZone();
                pWorld->bHidePlayer = 0;
                pWorld->UpdateCamera();
                pWorld->nFrameMode = 3;
                DrawGameArea(0);
                bMouseCaptured = 0;
                bMapTeleportEnabled = 0;
                break;
            case ZONE_TYPE_FINAL_DESTINATION:
            case ZONE_TYPE_ITEM_FOR_ITEM:
            case ZONE_TYPE_FIND_USEFUL_NPC:
            case ZONE_TYPE_ITEM_TO_PASS:
            case ZONE_TYPE_FROM_ANOTHER_MAP:
            case ZONE_TYPE_TO_ANOTHER_MAP:
            {
                {
                    CString str;
                    if (pWorld->mapGrid[gy * 10 + gx].flagA == 1)
                    {
                        str.LoadString(0xe01a);
                    }
                    else
                    {
                        short w = pWorld->mapGrid[gy * 10 + gx].cellItemA;
                        if (w >= 0)
                        {
                            Tile *pTile = (Tile *)pWorld->tiles.GetAt(w);
                            str.LoadString(0xe00e);
                            CString strKind;
                            if (pTile->flags & TILE_PUZZLE_ITEM_1)
                            {
                                strKind.LoadString(0xe016);
                            }
                            else if (pTile->flags & TILE_PUZZLE_ITEM_2)
                            {
                                strKind.LoadString(0xe017);
                            }
                            else if (pTile->flags & TILE_PUZZLE_ITEM_SEED_END)
                            {
                                strKind.LoadString(0xe018);
                            }
                            else if (pTile->flags & TILE_KEYCARD)
                            {
                                if (w == 0x213 || w == 0x285 || w == 0x43f || w == 0x433)
                                    strKind.LoadString(0xe016);
                                else
                                    strKind.LoadString(0xe019);
                            }
                            str += strKind;
                        }
                    }
                    ShowTextDialog(str, nBalloonX, nBalloonY, 1);
                }
                bMouseCaptured = 0;
                break;
            }
            case ZONE_TYPE_FINAL_ITEM:
            {
                {
                    CString str;
                    if (pWorld->mapGrid[gy * 10 + gx].flagB == 1)
                        str.LoadString(0xe014);
                    else
                        str.LoadString(0xe015);
                    ShowTextDialog(str, nBalloonX, nBalloonY, 1);
                }
                bMouseCaptured = 0;
                break;
            }
            case ZONE_TYPE_MAP_START:
            {
                {
                    CString str;
                    str.LoadString(0xe013);
                    ShowTextDialog(str, nBalloonX, nBalloonY, 1);
                }
                bMouseCaptured = 0;
                break;
            }
            case ZONE_TYPE_MAP_TO_ITEM_FOR_LOCK:
            {
                {
                    CString str;
                    if (pWorld->mapGrid[gy * 10 + gx].flagA == 1
                        && pWorld->mapGrid[gy * 10 + gx].flagB == 1)
                    {
                        str.LoadString(0xe00d);
                    }
                    else
                    {
                        short w = pWorld->mapGrid[gy * 10 + gx].cellItemA;
                        if (w >= 0)
                        {
                            Tile *pTile = (Tile *)pWorld->tiles.GetAt(w);
                            str.LoadString(0xe00e);
                            CString strKind;
                            if (pTile->flags & TILE_PUZZLE_ITEM_1)
                            {
                                strKind.LoadString(0xe016);
                            }
                            else if (pTile->flags & TILE_PUZZLE_ITEM_2)
                            {
                                strKind.LoadString(0xe017);
                            }
                            else if (pTile->flags & TILE_PUZZLE_ITEM_SEED_END)
                            {
                                strKind.LoadString(0xe018);
                            }
                            else if (pTile->flags & TILE_KEYCARD)
                            {
                                if (w == 0x213 || w == 0x285 || w == 0x43f || w == 0x433)
                                    strKind.LoadString(0xe016);
                                else
                                    strKind.LoadString(0xe019);
                            }
                            str += strKind;
                        }
                    }
                    ShowTextDialog(str, nBalloonX, nBalloonY, 1);
                }
                bMouseCaptured = 0;
                break;
            }
            case ZONE_TYPE_FIND_USEFUL_DROP:
            {
                short w = pWorld->mapGrid[gy * 10 + gx].cellItemA;
                if (w >= 0)
                {
                    Tile *pTile = (Tile *)pWorld->tiles.GetAt(w);
                    CString str;
                    if (pWorld->mapGrid[gy * 10 + gx].flagA == 1)
                    {
                        str.LoadString(0xe00d);
                    }
                    else
                    {
                        str.LoadString(0xe00e);
                        str += pTile->name;
                        str += "...";
                    }
                    ShowTextDialog(str, nBalloonX, nBalloonY, 1);
                }
                bMouseCaptured = 0;
                break;
            }
            case ZONE_TYPE_FIND_USEFUL_BUILDING:
            {
                {
                    CString str;
                    if (pWorld->mapGrid[gy * 10 + gx].flagB == 1)
                    {
                        str.LoadString(0xe00d);
                    }
                    else
                    {
                        short w = pWorld->mapGrid[gy * 10 + gx].cellItemC;
                        if (w >= 0)
                        {
                            Tile *pTile = (Tile *)pWorld->tiles.GetAt(w);
                            str.LoadString(0xe00f);
                            CString strKind;
                            if (pTile->flags & TILE_LOCATOR)
                            {
                                strKind.LoadString(0xe010);
                            }
                            else if (pTile->flags & TILE_WEAPON)
                            {
                                strKind.LoadString(0xe012);
                            }
                            else if (pTile->flags & TILE_ITEM)
                            {
                                strKind.LoadString(0xe011);
                            }
                            str += strKind;
                        }
                    }
                    ShowTextDialog(str, nBalloonX, nBalloonY, 1);
                }
                bMouseCaptured = 0;
                break;
            }
            }
            break;
        }
        }
        break;
    case 9:
    {
        int cy = (pWorld->nViewTop - pWorld->rectUnk3274.top + point.y) / 32;
        int cx = (pWorld->nViewLeft - pWorld->rectUnk3274.left + point.x) / 32;
        if (cx == nPickupX && nPickupY == cy)
            bPickupClickPendingMaybe = 1;
        else
            bPickupClickPendingMaybe = 0;
        break;
    }
    }
}

// FUNCTION: YODA 0x00412250
// GameView::OnLButtonUp — WM_LBUTTONUP by frame mode. Entry always clears the
// bump latch, closes a shown text dialog, releases capture and samples MK_SHIFT.
// Mode 2: unlock back to mode 3. Mode 3: scrollbar = plain return (sic: the only
// exit that skips Default()); inventory click = start dragging the slot's tile
// (the locator instead opens the world map: DrawLocatorMap, mode 7 reason 4) —
// dragging the detonator beeps 0x21, Artoo randomly beeps 0x2a/0x2b; a viewport
// click just re-arms mode 3; otherwise cancel a stale drag. Mode 4 (drop):
// weapon box = equip (the OnDragItem weapon-loop clone; drop-on-detonator
// buzzes), health dial = use health pack (+100/50/25) / harmful item (-50!) /
// note Artoo, arrow box / outside = flag it, viewport = repaint player cell;
// then fire OnDragItem at the translated coords. Mode 9: click the queued
// pickup = AddItemToInv, stamp flagB when it is the cell's reward, clear the
// spot object + tile, redraw, mode 3.
// EFFECTIVE MATCH (align 274, 705/701 insns; verify DIFF includes table noise).
// Cracks that landed: every skip path is a plain `break` to ONE trailing
// Default() — the per-case `bMouseCaptured = 0;` copies cross-jump into the
// single store before it (my Default();return; copies were +63 insns); the
// non-weapon buzz arm is the ELSE of `if (flags & TILE_WEAPON)` (deferred past
// the equip block, ends in ret); `int nOff = point.y - top;` computed BEFORE
// the scrollPos clamp. Parked: the heal ladder emits two 0x32 arms + jg-to-0
// polarity where the orig shares one 0x32 with jle-to-50 (same
// canonicalization residual as OnDragItem's ladder — lesson #6 family, G1);
// 0x12/0x1fe arms load/TEST/store drift; the MK_SHIFT store sink; case-9/
// arrow-box reg roles; cmp-direction on the loop backedge (both spellings
// canonicalize).
void CDeskcppView::OnLButtonUp(UINT nFlags, CPoint point)
{
    bBlockBumpUntilClick = 0;
    if (bTextDialogShown != 0)
    {
        bTextDialogShown = 0;
        bInputLocked = 0;
    }
    bMouseCaptured = 0;
    ReleaseCapture();
    bShiftHeld = 1;
    if ((nFlags & 4) == 0)
        bShiftHeld = 0;
    switch (pWorld->nFrameMode)
    {
    case 2:
        if (bInputLocked == 0)
            pWorld->nFrameMode = 3;
        bMouseCaptured = 0;
        break;
    case 3:
    {
        RECT rc;
        rc.left = pWorld->rectUnk3274.left - 12;
        rc.right = pWorld->rectUnk3274.right + 12;
        rc.top = pWorld->rectUnk3274.top - 12;
        rc.bottom = pWorld->rectUnk3274.bottom + 12;
        if (PtInRect(&pWorld->rectInvScroll, point) != 0)
            return;     // sic: the one exit that skips Default()
        if (PtInRect(&pWorld->rectUnk3284, point) != 0)
        {
            if (bInvClickPending == 0)
                break;
            int nOff = point.y - pWorld->rectUnk3284.top;
            int nPos = pInvScrollBar->scrollPos;
            if (nPos < 0)
                nPos = 0;
            int nSlot = nPos + nOff / 32;
            if (pWorld->inventory.GetSize() <= nSlot || bBusy != 0)
                break;
            Tile *pTile = ((InvItem *)pWorld->inventory.GetAt(nSlot))->pTile;
            if (pTile->flags == (TILE_GAME_OBJECT | TILE_ITEM | TILE_LOCATOR))
            {
                if (bDragActive == 1)
                {
                    bDragActive = 0;
                    UpdateDragCursor(1);
                }
                PlaySound(0x22);
                bMapTeleportEnabled = 0;
                CDC *pDC = GetDC();
                CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
                pMapReturnZone = pWorld->currentZone;
                bMapViewOpen = 1;
                pWorld->DrawLocatorMap(pDC, bBlinkState, bMapTeleportEnabled);
                pDC->SelectPalette(pOldPal, 0);
                ReleaseDC(pDC);
                pWorld->nFrameMode = 7;
                pWorld->nMapChangeReason = 4;
                nSavedCameraX = pWorld->cameraX;
                nSavedCameraY = pWorld->cameraY;
                bMapAtCanvasOriginMaybe = 1;
                bMouseCaptured = 0;
                strCheatBuffer = "";
                return;
            }
            SetCursor(0);
            nDragSlot = (short)nSlot;
            bDragActive = 1;
            draggedTile = pTile;
            memcpy(pDragTileCanvas->GetData(), draggedTile->pixels, 0x400);
            if ((Tile *)pWorld->tiles.GetAt(0x202) == draggedTile)
                PlaySound(0x21);
            if ((Tile *)pWorld->tiles.GetAt(0x31a) == draggedTile)
            {
                if (rand() % 2 == 0)
                    PlaySound(0x2a);
                else
                    PlaySound(0x2b);
            }
            pWorld->nFrameMode = 4;
            DrawText(0);
            bMouseCaptured = 0;
            break;
        }
        if (PtInRect(&rc, point) != 0)
        {
            if (bInputLocked == 0)
                pWorld->nFrameMode = 3;
            bMouseCaptured = 0;
            break;
        }
        if (bDragActive == 1)
        {
            bDragActive = 0;
            UpdateDragCursor(1);
            nDragLastScreenY = -1;
            nDragLastScreenX = -1;
            pWorld->nFrameMode = 3;
            nDragSlot = -1;
        }
        break;
    }
    case 4:
    {
        if (bDragActive != 1)
            break;
        int dx = pWorld->nViewLeft - pWorld->rectUnk3274.left + point.x;
        int dy = pWorld->nViewTop - pWorld->rectUnk3274.top + point.y;
        bDragActive = 0;
        UpdateDragCursor(1);
        nDragSlot = -1;
        nDragLastScreenY = -1;
        nDragLastScreenX = -1;
        pWorld->nFrameMode = 3;
        if (PtInRect(&pWorld->rectWeaponBox, point) != 0)
        {
            if (draggedTile->flags & TILE_WEAPON)
            {
            int bFound = 0;
            int nCount = pWorld->characters.GetSize();
            if (nCount > 0)
            {
                Character **paChars = (Character **)pWorld->characters.GetData();
                int i = 0;
                do
                {
                    Character *pChar = *paChars;
                    int nCharTile;
                    if (pChar != 0 && (nCharTile = pChar->frames[7]) >= 0
                        && (Tile *)pWorld->tiles.GetAt(nCharTile) == draggedTile)
                    {
                        if (pChar == pWorld->currentWeapon)
                        {
                            pChar->unk48 = pWorld->currentWeapon->unk48;
                        }
                        else switch (nCharTile)
                        {
                        case 0x12:
                            if ((pChar->unk48 = pWorld->ammoLightsaber) == 0)
                                pChar->unk48 = 30;
                            PlaySound(0x1f);
                            break;
                        case 0x1fe:
                            if ((pChar->unk48 = pWorld->ammoTheForce) == 0)
                                pChar->unk48 = 30;
                            PlaySound(0x1f);
                            break;
                        case 0x1ff:
                        {
                            short *p = &pWorld->weaponState[0];
                            short a = *p;
                            if (a <= 0)
                            {
                                *p = 15;
                                a = pWorld->weaponState[0];
                            }
                            pChar->unk48 = a;
                            PlaySound(0x34);
                            break;
                        }
                        case 0x200:
                        {
                            short *p = &pWorld->weaponState[1];
                            short a = *p;
                            if (a <= 0)
                            {
                                *p = 30;
                                a = pWorld->weaponState[1];
                            }
                            pChar->unk48 = a;
                            PlaySound(0x20);
                            break;
                        }
                        case 0x201:
                        {
                            short *p = &pWorld->weaponState[2];
                            short a = *p;
                            if (a <= 0)
                            {
                                *p = 10;
                                a = pWorld->weaponState[2];
                            }
                            pChar->unk48 = a;
                            PlaySound(0x20);
                            break;
                        }
                        case 0x204:
                        {
                            short *p = &pWorld->weaponState[3];
                            short a = *p;
                            if (a <= 0)
                            {
                                *p = 15;
                                a = pWorld->weaponState[3];
                            }
                            pChar->unk48 = a;
                            break;
                        }
                        case 0x205:
                        {
                            short *p = &pWorld->nCurrentAmmoMaybe;
                            short a = *p;
                            if (a <= 0)
                            {
                                *p = 15;
                                a = pWorld->nCurrentAmmoMaybe;
                            }
                            pChar->unk48 = a;
                            break;
                        }
                        }
                        if (nCharTile == 0x202)
                        {
                            PlaySound(6);
                            draggedTile = 0;
                            bMouseCaptured = 0;
                            DrawText(0);
                            return;
                        }
                        bFound = 1;
                        pWorld->currentWeapon = pChar;
                        pWorld->unk2e30 = i + 8;
                        draggedTile = 0;
                        bMouseCaptured = 0;
                        DrawText(0);
                        break;
                    }
                    paChars++;
                    i++;
                } while (i < nCount);
            }
            if (bFound == 0)
            {
                PlaySound(6);
                draggedTile = 0;
                bMouseCaptured = 0;
                DrawText(0);
                pWorld->currentWeapon = 0;
            }
            DrawWeaponBox(0);
            DrawWeaponIcon(0);
            return;
            }
            else
            {
                PlaySound(6);
                draggedTile = 0;
                bMouseCaptured = 0;
                DrawText(0);
                bArtooBeepPending0Maybe = 0;
                return;
            }
        }
        if (PtInRect(&pWorld->rectHealthDial, point) != 0)
        {
            Tile *pTile = draggedTile;
            if (pTile->flags & TILE_HEALTH_PACK)
            {
                if (pWorld->healthLo == 1 && pWorld->healthHi == 1)
                {
                    PlaySound(6);
                    bMouseCaptured = 0;
                    draggedTile = 0;
                    DrawText(0);
                    return;
                }
                int nHeal = pWorld->FindTile(pTile);
                if (nHeal <= 0x1fa)
                {
                    if (nHeal < 0x1f9)
                    {
                        if (nHeal == 0x1e0 || nHeal == 0x1e2)
                            nHeal = 50;
                        else
                            nHeal = 0;
                    }
                    else
                    {
                        nHeal = 100;
                    }
                }
                else if (nHeal != 0x1fb)
                {
                    if (nHeal < 0x4ac || nHeal > 0x4ae)
                        nHeal = 0;
                    else
                        nHeal = 50;
                }
                else
                {
                    nHeal = 25;
                }
                AddHealth(nHeal);
                PlaySound(0);
                RemoveItem(draggedTile);
                bMouseCaptured = 0;
                draggedTile = 0;
                DrawText(0);
                return;
            }
            if (pTile->flags & TILE_ITEM_HARMFUL_MAYBE)
            {
                AddHealth(-50);
                PlaySound(4);
                RemoveItem(draggedTile);
                bMouseCaptured = 0;
                return;
            }
            if (pWorld->FindTile(pTile) == 0x31a)
                bDropOnArtooMaybe = 1;
            else
                bDropOnArtooMaybe = 0;
        }
        else if (PtInRect(&pWorld->rectArrowBox, point) != 0)
        {
            if (pWorld->FindTile(draggedTile) == 0x31a)
                bDraggedArtooBlockedMaybe = 1;
            else
                bDraggedArtooBlockedMaybe = 0;
        }
        else if (PtInRect(&pWorld->rectUnk3274, point) == 0)
        {
            bDropOutsideViewMaybe = 1;
        }
        else
        {
            DrawZoneCell(pWorld->cameraX / 32, pWorld->cameraY / 32);
            DrawGameArea(0);
            bMouseCaptured = 0;
            bDropOutsideViewMaybe = 0;
        }
        OnDragItem(dx, dy, draggedTile);
        draggedTile = 0;
        DrawText(0);
        bMouseCaptured = 0;
        break;
    }
    case 9:
    {
        if (bPickupClickPendingMaybe == 0 || bBusy != 0)
            break;
        bPickupClickPendingMaybe = 0;
        int cx = (pWorld->nViewLeft - pWorld->rectUnk3274.left + point.x) / 32;
        int cy = (pWorld->nViewTop - pWorld->rectUnk3274.top + point.y) / 32;
        if (nPickupX != cx || nPickupY != cy || nPickupTileId < 0)
            break;
        AddItemToInv((Tile *)pWorld->tiles.GetAt(nPickupTileId));
        short w = pWorld->mapGrid[pWorld->playerY * 10 + pWorld->playerX].cellItemC;
        if (w >= 0 && (int)w == nPickupTileId)
            pWorld->mapGrid[pWorld->playerY * 10 + pWorld->playerX].flagB = 1;
        if (pPickupObj != 0)
        {
            pPickupObj->state = 0;
            pWorld->currentZone->SetTile(nPickupX, nPickupY, 1, -1);
        }
        DrawTileAt((short)cx, (short)cy, -1);
        DrawGameArea(0);
        pWorld->nFrameMode = 3;
        bMouseCaptured = 0;
        break;
    }
    }
    Default();
}

// FUNCTION: YODA 0x00412cc0
// [EFFECTIVE-WIP: align=130/reg_pen=6 over 413/411 insns — ONE allocator decision + its fallout.
//  (a) The >8bpp pixel loop: orig assigns i=EDI/x2=ESI/y2=EBX and calls SetPixel through the
//      import slot each iteration; ours caches the import in EDI, kicking i->EBX and homing y2
//      to a frame slot (inc/cmp dword [ebp-x]) — the v24 "import-pointer caching flips with the
//      restructure" family. Decl-order probes (i/y2/x2 hoists) ALL inert (usage-count-driven).
//  (b) The first guard loads x->EAX/y->ECX vs orig x->ECX/y->EAX (rank tie-break); downstream
//      the first SetBitmapBits reloads pBitmap from its slot in orig vs our live-reg reuse, and
//      `cmp [ebp+8],0` schedules one insn earlier. Value-local probe (x0=*pX) WORSE (reg_pen 14),
//      cmp-flip inert. Minimal-TU probe: IDENTICAL score solo => header-dial/tie-break, not
//      TU-position — G1 fodder.
//  ⭐ SOLVED here: field-to-field memcpy => LONE `rep movsb` (see the in-body comment).]
// Rebuilds the drag-cursor overlay. Restores the 32x32 screen block saved at the previous
// drag position (paDragSaveBits blitted back via a 1-plane DDB), records the new position
// (mouse - 16,16), then unless bClear grabs the screen under the new position and composites
// the dragged tile over it — Canvas::BlitMasked at <=8bpp (palette path), or a
// GetPaletteEntries+SetPixel per-pixel loop at hi-color (paDragSaveBits2 path). TRY/CATCH
// guards the `new CBitmap` (fatal 0xE01E box + AfxAbort on OOM, no THROW_LAST here).
// sic: pBitmap leaks when CBitmap::Attach fails (early return skips the delete).
void CDeskcppView::UpdateDragCursor(int bClear)
{
    ::SetCursor(NULL);
    HWND *phWnd = &m_hWnd;
    CDC *pDC = CDC::FromHandle(::GetDC(*phWnd));
    if (pDC == NULL)
        return;
    CDeskcppDoc **ppWorld = &pWorld;
    CPalette *pOldPal = pDC->SelectPalette((*ppWorld)->pPalette, 0);
    CDC dcMem;
    if (!dcMem.Attach(::CreateCompatibleDC(NULL)))
        return;
    CBitmap *pBitmap;
    TRY {
        pBitmap = new CBitmap;
    }
    }              // closes the try block the TRY macro opened
    catch (CException *e) {                // hand-expanded CATCH_ALL(e)
        _afxExceptionLink.m_pException = e;
        AfxMessageBox(0xe01e, 0, (UINT)-1);
        AfxAbort();
    }
    }              // closes the TRY macro's outer (link-scope) brace
    int nBpp = dcMem.GetDeviceCaps(BITSPIXEL);
    if (!pBitmap->Attach(::CreateBitmap(32, 32, 1, nBpp, NULL)))
        YODA_SIC_RETURN(BUGLOG(("sic UpdateDragCursor: CBitmap::Attach failed, pBitmap freed\n")); delete pBitmap;) // sic: pBitmap leak (early return skips the delete)
    CBitmap *pOldBmp = dcMem.SelectObject(pBitmap);
    int *pX = &nDragLastScreenX;
    if (*pX >= 0)
    {
        int *pY = &nDragLastScreenY;
        if (*pY >= 0 && *pX < 509 && *pY < 314)
        {
            ::SetBitmapBits((HBITMAP)pBitmap->m_hObject, (nBpp / 8) << 10, paDragSaveBits);
            ::BitBlt(pDC->m_hDC, *pX, *pY, 32, 32, dcMem.m_hDC, 0, 0, SRCCOPY);
        }
    }
    int x = nMouseX - 16;
    int y = nMouseY - 16;
    *pX = x;
    nDragLastScreenY = y;
    if (bClear == 0)
    {
        Tile **ppTile = &draggedTile;
        if (*ppTile != NULL)
        {
            int cx = x;
            if (x < 0)
                cx = 0;
            int cy = y;
            if (y < 0)
                cy = 0;
            if (x >= 0 && y >= 0 && x < 509 && y < 314)
            {
                if (nBpp <= 8)
                {
                    Canvas **ppCanvas = &pDragTileCanvas;
                    (*ppCanvas)->SetPalette(0, 0x100, (RGBQUAD *)(*ppWorld)->pSysColorTable);
                    ::BitBlt(dcMem.m_hDC, 0, 0, 32, 32, pDC->m_hDC, cx, cy, SRCCOPY);
                    void *pData = (*ppCanvas)->GetData();
                    int nPP = nBpp / 8;
                    int nSize = nPP << 10;
                    ::GetBitmapBits((HBITMAP)pBitmap->m_hObject, nSize, pData);
                    memcpy(paDragSaveBits, (*ppCanvas)->GetData(), nSize);
                    (*ppCanvas)->BlitMasked((char *)(*ppTile)->pixels, 32, 32, 0, 0, 0);
                    ::SetBitmapBits((HBITMAP)pBitmap->m_hObject, nPP << 10, (*ppCanvas)->GetData());
                }
                else
                {
                    int y2 = 0;
                    if (paDragSaveBits2 != NULL)
                    {
                        ::BitBlt(dcMem.m_hDC, 0, 0, 32, 32, pDC->m_hDC, cx, cy, SRCCOPY);
                        int nSize = (nBpp / 8) << 10;
                        ::GetBitmapBits((HBITMAP)pBitmap->m_hObject, nSize, paDragSaveBits2);
                        // field-to-field memcpy → VC4.2 emits the LONE rep movsb form
                        // (call-result/param/global operands get the movsd+movsb split —
                        //  proven by probe battery 2026-07-07; see CLAUDE.md v25 notes)
                        memcpy(paDragSaveBits, paDragSaveBits2, nSize);
                        int i = 0;
                        do
                        {
                            int x2 = 0;
                            do
                            {
                                BYTE b = (*ppTile)->pixels[i];
                                i++;
                                if (b != 0)
                                {
                                    PALETTEENTRY pe;
                                    ::GetPaletteEntries((HPALETTE)(*ppWorld)->pPalette->m_hObject, b, 1, &pe);
                                    ::SetPixel(dcMem.m_hDC, x2, y2, RGB(pe.peRed, pe.peGreen, pe.peBlue));
                                }
                                x2++;
                            } while (x2 < 32);
                            y2++;
                        } while (y2 < 32);
                    }
                }
                ::BitBlt(pDC->m_hDC, cx, cy, 32, 32, dcMem.m_hDC, 0, 0, SRCCOPY);
            }
        }
    }
    dcMem.SelectObject(pOldBmp);
    pDC->SelectPalette(pOldPal, 0);
    ::ReleaseDC(*phWnd, pDC->m_hDC);
    delete pBitmap;
}

// FUNCTION: YODA 0x004131a0
// [EFFECTIVE: align=30/reg_pen=10 over 278/273 insns — two residuals, both tie-break family:
//  (1) arm-1's `if (nFrameMode == 4) {=3; return;}`: orig emits jne->0x41354a (the !=4 return
//      cross-jumped into the FUNCTION-END shared return-1) with the store arm inline
//      (store folded INTO its epilogue after pop edi); ours defers the arm (je) and inlines
//      the fall-through return copy (+5 insns). Probed: inner trailing return / if-else /
//      plain-if (arm-2's shape) all WORSE (60/60/40) — the nested fall-out form (30) is the
//      local optimum. Note arm-2's IDENTICAL logic uses the plain-if LOCAL-jne shape — the
//      two arms provably differ in source shape.
//  (2) nHitTest rides EAX vs orig ECX from the first load (this vacates ECX to ESI either
//      way), cascading r-swaps through the nFrameMode compare chain (reg_pen 10).
//  ⭐ Crack that got it from 98->30: PtInRect takes *(POINT *)&nMouseX — the adjacent
//     nMouseX/nMouseY fields ARE the POINT (a `POINT pt` local costs 8 frame bytes + stores).]
// WM_SETCURSOR: picks the cursor by game state. Suppresses it during drags outside the
// client (HTNOWHERE/HTERROR), forces the arrow while busy/map-view/modes 5+7, the wand at
// mode 9, hides it at mode 6, cancels a drag when the mouse leaves the play area (both
// cancel arms are REAL duplicated dev code — their nFrameMode 4->3 tails differ in shape),
// and otherwise picks one of 8 direction cursors from (nMoveDX,nMoveDY), updating the
// player's facing frame tile through the Character GetFrameTile/GetWalkFrameTile pair.
BOOL CDeskcppView::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message)
{
    if (bViewActive == 0)
    {
        Default();
        return TRUE;
    }
    if ((nHitTest == HTNOWHERE || nHitTest == (UINT)HTERROR) && bDragActive != 0)
        return FALSE;
    if (pWorld->bWorldReadyMaybe != 0 || bMapViewOpen != 0 ||
        pWorld->nFrameMode == 5 || pWorld->nFrameMode == 7)
    {
        ::SetCursor(hCursor);
        return TRUE;
    }
    if (pWorld->nFrameMode == 9)
    {
        ::SetCursor(hCursor11);
        return TRUE;
    }
    if (pWorld->nFrameMode == 6)
    {
        ::SetCursor(NULL);
        return TRUE;
    }
    if (nHitTest != HTCLIENT && bMouseCaptured == 0)
    {
        if (bDragActive == 1)
        {
            bDragActive = 0;
            UpdateDragCursor(0);
            UpdateDragCursor(1);
            nDragSlot = -1;
            DrawText(NULL);
            ::SetCursor(hCursor);
            if (pWorld->nFrameMode == 4)
            {
                pWorld->nFrameMode = 3;
                return TRUE;
            }
        }
        return TRUE;
    }
    if (bDragActive == 1)
    {
        if ((nMouseX <= 0x1da || nMouseY >= 0xf0) && nMouseY > 0xe &&
            nMouseX > 0xe && nMouseY < 0x124 && nMouseX < 0x1fa)
        {
            UpdateDragCursor(0);
            ::SetCursor(NULL);
            return TRUE;
        }
        bDragActive = 0;
        UpdateDragCursor(0);
        UpdateDragCursor(1);
        nDragSlot = -1;
        DrawText(NULL);
        ::SetCursor(hCursor);
        if (pWorld->nFrameMode == 4)
            pWorld->nFrameMode = 3;
        return TRUE;
    }
    if (bKeyboardMoveActive == 1)
    {
        ::SetCursor(NULL);
        return TRUE;
    }
    RECT rc;
    rc.left = pWorld->rectUnk3274.left - 12;
    rc.right = pWorld->rectUnk3274.right + 12;
    rc.top = pWorld->rectUnk3274.top - 12;
    rc.bottom = pWorld->rectUnk3274.bottom + 12;
    if (!::PtInRect(&rc, *(POINT *)&nMouseX) && bMouseCaptured == 0)
    {
        if (bDragActive == 1)
            UpdateDragCursor(1);
        ::SetCursor(hCursor);
        return TRUE;
    }
    HCURSOR h;
    if (nMoveDX == -1 && nMoveDY == -1)
        h = hCursor2;
    else if (nMoveDX == -1 && nMoveDY == 0)
        h = hCursor3;
    else if (nMoveDX == -1 && nMoveDY == 1)
        h = hCursor4;
    else if (nMoveDX == 0 && nMoveDY == -1)
        h = hCursor5;
    else if (nMoveDX == 0 && nMoveDY == 1)
        h = hCursor6;
    else if (nMoveDX == 1 && nMoveDY == -1)
        h = hCursor7;
    else if (nMoveDX == 1 && nMoveDY == 1)
        h = hCursor8;
    else if (nMoveDX == 1 && nMoveDY == 0)
        h = hCursor9;
    else
        h = hCursor10;
    ::SetCursor(h);
    Character *pChar = pWorld->pPlayerChar;
    if (pChar != NULL)
    {
        if (pWorld->nFrameMode == 2)
        {
            pWorld->pPlayerFrameTile = (Tile *)pChar->GetFrameTile(nMoveDX, nMoveDY, &pWorld->tiles, nWalkFramePhase);
            return TRUE;
        }
        pWorld->pPlayerFrameTile = (Tile *)pChar->GetWalkFrameTile(nMoveDX, nMoveDY, &pWorld->tiles);
    }
    return TRUE;
}


// FUNCTION: YODA 0x00413580
// [EFFECTIVE: align=130/reg_pen=14 over 424/423 insns (~91% bytes identical) — four known
//  parked families: (a) prologue scheduling (orig loads point.x BEFORE the reg pushes and
//  keeps `this` in ECX for the first store); (b) the nMoveDX/DY=0 zero stores: orig uses
//  imm-0 stores + xor for nEdges, ours reuses the zeroed reg (v3 imm-vs-reg store batching);
//  (c) rcOuter.bottom's init wedged INTO the PtInRect arg pushes in orig; (d) one extra
//  epilogue copy at the nEdges!=0 exit + the x/y load-order swap in the rcView copy wedge.
//  ⭐ Cracks that got it 344k->132k: pMouse POINT* pointer local over nMouseX/Y (role pin),
//  y-then-x decl order BEFORE the rcView struct copy, and the POSITIVE PtInRect nesting
//  `if (P_out) { if (!P_in) { if (P_cell) A else B } } else C` — then-arms inline,
//  else-arms deferred (C emitted last, its final edge falls into the epilogue).]
// WM_MOUSEMOVE: records the mouse position, computes the walk direction (nMoveDX/nMoveDY).
// Outside the play rect (uncaptured) it defers to DefWindowProc. At the play-rect edges
// (16px bands) it sets edge-scroll directions and bails. Otherwise it classifies the mouse
// 8-ways against three concentric rects around the camera/player cell (cell 32x32, inner
// inset 4, outer outset 8). The three classify blocks are copy-paste dev code — the CELL
// block's (-1,-1) and (1,1) corners store nMoveDY BEFORE nMoveDX (hand-edited copies);
// every arm returns except the OUTER block's final edge (falls off into the epilogue).
void CDeskcppView::OnMouseMove(UINT nFlags, CPoint point)
{
    POINT *pMouse = (POINT *)&nMouseX;
    nMovePending = 0;
    pMouse->x = point.x;
    nMoveCommand = -1;
    pMouse->y = point.y;
    if (bKeyboardMoveActive == 1 && pWorld->nFrameMode == 2)
        pWorld->nFrameMode = 3;
    bKeyboardMoveActive = 0;
    if (!::PtInRect(&pWorld->rectUnk3274, *pMouse) && bMouseCaptured == 0)
    {
        bMouseCaptured = 0;
        Default();
        return;
    }
    bMouseCaptured = 1;
    if ((nFlags & MK_LBUTTON) == 0)
        bMouseCaptured = 0;
    bShiftHeld = 1;
    if ((nFlags & MK_SHIFT) == 0)
        bShiftHeld = 0;
    int y = nMouseY;
    int x = nMouseX;
    RECT rcView = *(RECT *)&pWorld->nViewLeft;
    RECT rcPlay = pWorld->rectUnk3274;
    nMoveDY = 0;
    int nEdges = 0;
    nMoveDX = 0;
    if (rcPlay.left + 0x10 > x)
    {
        nMoveDX = -1;
        nEdges = 1;
    }
    else if (rcPlay.right - 0x10 < x)
    {
        nEdges = 1;
        nMoveDX = 1;
    }
    if (rcPlay.top + 0x10 > y)
    {
        nMoveDY = -1;
        nEdges++;
    }
    else if (rcPlay.bottom - 0x10 < y)
    {
        nMoveDY = 1;
        nEdges++;
    }
    if (nEdges == 0)
    {
        RECT rcCell;
        RECT rcInner;
        RECT rcOuter;
        rcCell.left = pWorld->cameraX - rcView.left + rcPlay.left;
        rcCell.top = pWorld->cameraY - rcView.top + rcPlay.top;
        rcCell.right = rcCell.left + 0x20;
        rcCell.bottom = rcCell.top + 0x20;
        rcInner.left = rcCell.left + 4;
        rcInner.right = rcCell.right - 4;
        rcInner.top = rcCell.top + 4;
        rcInner.bottom = rcCell.bottom - 4;
        rcOuter.left = rcCell.left - 8;
        rcOuter.right = rcCell.right + 8;
        rcOuter.top = rcCell.top - 8;
        rcOuter.bottom = rcCell.bottom + 8; YODA_SIC_FIX(nMoveDX = (x < rcInner.left) ? -1 : (x >= rcInner.right) ? 1 : 0; nMoveDY = (y < rcInner.top) ? -1 : (y >= rcInner.bottom) ? 1 : 0; return;)  /* YODA_BUGFIX: one clean 8-way partition around rcInner replaces the 3 nested shells below, whose per-shell corners each drop a 1px cardinal/'+' crack into the diagonal (cursor+facing flicker). Same '+' dead-zone (rcInner); returns before the original shells run. Anchor: YODA_SIC_FIX()->empty, tokens identical. */
        if (::PtInRect(&rcOuter, *pMouse))
        {
            if (!::PtInRect(&rcInner, *pMouse))
            {
                if (::PtInRect(&rcCell, *pMouse))
                {
                    if (x < rcInner.left && y < rcInner.top)
                    {
                        nMoveDX = -1;
                        nMoveDY = -1;
                        return;
                    }
                    if (x < rcInner.left && y > rcInner.bottom)
                    {
                        nMoveDX = -1;
                        nMoveDY = 1;
                        return;
                    }
                    if (x > rcInner.right && y < rcInner.top)
                    {
                        nMoveDX = 1;
                        nMoveDY = -1;
                        return;
                    }
                    if (x > rcInner.right && y > rcInner.bottom)
                    {
                        nMoveDX = 1;
                        nMoveDY = 1;
                        return;
                    }
                    if (x < rcInner.left)
                    {
                        nMoveDX = -1;
                        return;
                    }
                    if (x > rcInner.right)
                    {
                        nMoveDX = 1;
                        return;
                    }
                    if (y < rcInner.top)
                    {
                        nMoveDY = -1;
                        return;
                    }
                    if (y > rcInner.bottom)
                    {
                        nMoveDY = 1;
                        return;
                    }
                }
                else
                {
                    if (x < rcCell.left && y < rcCell.top)
                    {
                        nMoveDY = -1;
                        nMoveDX = -1;
                        return;
                    }
                    if (x < rcCell.left && y > rcCell.bottom)
                    {
                        nMoveDX = -1;
                        nMoveDY = 1;
                        return;
                    }
                    if (x > rcCell.right && y < rcCell.top)
                    {
                        nMoveDX = 1;
                        nMoveDY = -1;
                        return;
                    }
                    if (x > rcCell.right && y > rcCell.bottom)
                    {
                        nMoveDY = 1;
                        nMoveDX = 1;
                        return;
                    }
                    if (x < rcCell.left)
                    {
                        nMoveDX = -1;
                        return;
                    }
                    if (x > rcCell.right)
                    {
                        nMoveDX = 1;
                        return;
                    }
                    if (y < rcCell.top)
                    {
                        nMoveDY = -1;
                        return;
                    }
                    if (y > rcCell.bottom)
                    {
                        nMoveDY = 1;
                        return;
                    }
                }
            }
        }
        else
        {
            if (x < rcOuter.left && y < rcOuter.top)
            {
                nMoveDX = -1;
                nMoveDY = -1;
                return;
            }
            if (x < rcOuter.left && y > rcOuter.bottom)
            {
                nMoveDX = -1;
                nMoveDY = 1;
                return;
            }
            if (x > rcOuter.right && y < rcOuter.top)
            {
                nMoveDX = 1;
                nMoveDY = -1;
                return;
            }
            if (x > rcOuter.right && y > rcOuter.bottom)
            {
                nMoveDX = 1;
                nMoveDY = 1;
                return;
            }
            if (x < rcOuter.left)
            {
                nMoveDX = -1;
                return;
            }
            if (x > rcOuter.right)
            {
                nMoveDX = 1;
                return;
            }
            if (y < rcOuter.top)
            {
                nMoveDY = -1;
                return;
            }
            if (y > rcOuter.bottom)
                nMoveDY = 1;
        }
    }
}

// FUNCTION: YODA 0x00413b20
// [EFFECTIVE: align=20/reg_pen=0 — ONLY the two end stubs are swapped: orig emits the EH
//  handler thunk (mov eax,FuncInfo; jmp __CxxFrameHandler) BEFORE the ~CBrush call stub
//  (lea ecx; jmp ??1CBrush); ours emits dtor-stub-then-handler. Identical bytes, block
//  order only — funclet-ordering axis, not source-steerable (UpdateDragCursor's stubs
//  aligned with the same source patterns). Crack: h/w as locals (h FIRST) so both
//  subtractions batch before the PATCOPY push.]
// WM_ERASEBKGND: fills the clip box with the 3D-face system color via a PatBlt.
// `this` is never read — everything goes through the passed CDC.
BOOL CDeskcppView::OnEraseBkgnd(CDC *pDC)
{
    CBrush br(::GetSysColor(COLOR_BTNFACE));
    CBrush *pOldBrush = pDC->SelectObject(&br);
    RECT rc;
    pDC->GetClipBox(&rc);
    int h = rc.bottom - rc.top;
    int w = rc.right - rc.left;
    ::PatBlt(pDC->m_hDC, rc.left, rc.top, w, h, PATCOPY);
    pDC->SelectObject(pOldBrush);
    return TRUE;
}

// FUNCTION: YODA 0x00413be0
// Genuinely empty method, called from OnTimer each frame (a stubbed-out per-frame hook).
void CDeskcppView::EmptyFrameHookMaybe()
{
}

// FUNCTION: YODA 0x00413bf0
// Flushes channel 0 of the WAVMIX session (stops the currently queued sound effects).
void CDeskcppView::SoundFlush()
{
    if (soundSession != 0)
        WaveMixFlushChannel(soundSession, 0, 1);
}

// FUNCTION: YODA 0x00413c10
// WM_RBUTTONDOWN. When playing (nFrameMode 3) with a weapon equipped, resolves a cardinal
// fire direction from the current movement delta (diagonals snap to up/left/right), bails if
// firing would leave the visible zone, else latches a shot (nFrameMode 8). When in the map
// overlay (nFrameMode 7) with the game running, closes the map and restores the play zone.
void CDeskcppView::OnRButtonDown(UINT nFlags, CPoint point)
{
    switch (pWorld->nFrameMode) {
    case 3:
        if (pWorld->currentWeapon != 0) {
            if (nMoveDX == 0 || nMoveDY == 0) {
                nFireDirX = nMoveDX;
                nFireDirY = nMoveDY;
            } else if ((nMoveDX < 0 && nMoveDY < 0) || (nMoveDX == 1 && nMoveDY < 0)) {
                nFireDirX = 0;
                nFireDirY = -1;
            } else if (nMoveDX < 0 && nMoveDY == 1) {
                nFireDirX = -1;
                nFireDirY = 0;
            } else if (nMoveDX == 1 && nMoveDY == 1) {
                nFireDirX = 1;
                nFireDirY = 0;
            }

            if (nFireDirX != 0) {
                if (nFireDirX < 0) {
                    if (pWorld->cameraX < 0x20)
                        return;
                } else if (nFireDirX > 0) {
                    if ((pWorld->currentZone->width - 2) * 32 < pWorld->cameraX)
                        return;
                }
                nFireStep = 0;
                bFireKeyLatchMaybe = 1;
                pWorld->nFrameMode = 8;
                return;
            }
            if (nFireDirY != 0) {
                if (nFireDirY < 0) {
                    if (pWorld->cameraY < 0x20)
                        return;
                } else if (nFireDirY > 0) {
                    if ((pWorld->currentZone->height - 2) * 32 < pWorld->cameraY)
                        return;
                }
                nFireStep = 0;
                bFireKeyLatchMaybe = 1;
                pWorld->nFrameMode = 8;
                return;
            }
        }
        break;
    case 7:
        if (pWorld->nMapChangeReason == 4) {
            pWorld->currentZone = pMapReturnZone;
            bMapViewOpen = 0;
            pWorld->RefreshZone();
            pWorld->bHidePlayer = 0;
            pWorld->UpdateCamera();
            pWorld->nFrameMode = 3;
            DrawGameArea(NULL);
            bMouseCaptured = 0;
            bMapTeleportEnabled = 0;
        }
        break;
    }
}

// FUNCTION: YODA 0x00413dd0
// Redraws the zone cell at the camera position. NOTE the <<5: DrawZoneCell takes CELL
// coords and multiplies by 32 itself — passing camera*32 here draws a far-off cell unless
// camera is 0 (sic? name keeps Maybe).
void CDeskcppView::RedrawPlayerCellMaybe()
{
    int x = pWorld->cameraX << 5;
    int y = pWorld->cameraY << 5;
    DrawZoneCell(x, y);
}

// FUNCTION: YODA 0x00413df0
// [EFFECTIVE-WIP: align=1066/reg_pen=126, byte_diff 730/4635 (~84% bytes) — first-pass. Solved
//  on the way (2.02M->1.08M): all GetTile/GetZoneCell results route through INT locals with
//  (short) casts (movsx-immediately, incl. `int nT/nCell` per edge case); case arms are
//  `if (nCell >= 0) {big} else PlaySound(6);` (else lands at case end); `UINT nFlags =
//  pTile->flags` is a REAL two-use register local (character + push tests — the
//  FireWeaponStep flags-test axis CONFIRMED as two-use); the push guard is FLAT
//  (`if (bPush && (nFlags&8)) {..} if (bPush) break;` — one-deep test elimination jumps
//  the JZ past the second if); char arm = guard-style breaks with the enemy test FIRST;
//  pickup arm stores bPush=1 INSIDE the if; pull block uses negated int locals (ndx/ndy)
//  + (short) casts at the DrawZoneCell site; DrawZoneCell player-cell sites take (cx,cy)
//  ints (NO short locals — Ghidra's sVar4/y are phantom).
//  REMAINING (parked): (a) this rides EDI vs orig ESI from the prologue (push-order swap,
//  global r-fallout); (b) the (nMask & 0x2a) dialog arm inlines in ours vs deferred-to-end
//  in orig — the arm owns the shared DrawPlayer/DrawGameArea tail in orig (the v24
//  "merge-partner not steerable" family); (c) six GetFrameTile sites have 1-insn
//  mov/push order swaps; (d) scattered store-batching. G1 fodder.]
// The bump/walk handler (IACT event 2 = BumpTile): moves the player one cell in (dx,dy),
// running the whole interaction pipeline — zone-edge transitions (4 duplicated arms, one per
// direction), bump-object dispatch (pickup/vehicle/door/lock/teleporter/xwing), bump scripts
// via Zone::IactRun, enemy contact damage, shift-push (straight) and shift-pull (diagonal)
// of TILE_PUSH_PULL_BLOCK tiles, and the IactProbeMove auto-slide retry loop (cases 2-5
// adjust the delta and loop). Arms end with their own duplicated DrawPlayer/DrawGameArea
// tails (cl cross-jumps them into the 0x414f5b/0x414f63 shared blocks).
void CDeskcppView::OnBumpTile(int dx, int dy)
{
    int dx2 = dx;
    int dy2 = dy;
    if (bInputLocked == 0 && bBlockBumpUntilClick == 0)
    {
        bInputLocked = 1;
        bBusy = 1;
        if (pWorld->nFrameMode != 6 && pWorld->nFrameMode != 1)
        {
            if (bRearmHotspotsMaybe == 1)
            {
                ReenableHotspotObjects();
                bRearmHotspotsMaybe = 0;
            }
            int cx = pWorld->cameraX / 32;
            int cy = pWorld->cameraY / 32;
            for (;;)
            {
                int tx = dx2 + cx;
                int ty = dy2 + cy;
                int bPush = 0;
                if (pWorld->unk3378 != 0)
                {
                    if (pWorld->currentZone->GetEdgeCode(tx, ty) == 0)
                    {
                        if (pWorld->bHidePlayer == 0)
                            DrawZoneCell(cx, cy);
                        pWorld->cameraX = tx * 32;
                        pWorld->cameraY = ty * 32;
                        pWorld->UpdateCamera();
                        pWorld->DrawPlayer();
                        DrawGameArea(NULL);
                    }
                    else
                    {
                        if (pWorld->bHidePlayer == 0)
                            DrawZoneCell(cx, cy);
                        UpdatePlayerWalkFrame();
                    }
                    break;
                }
                if (bShiftHeld != 0)
                {
                    if (dx == 0 || dy == 0)
                        bPush = 1;
                    else
                        bPush = 0;
                }
                UpdateItemObjectsMaybe();
                if (dx2 == 0 && dy2 == 0)
                {
                    UpdatePlayerWalkFrame();
                    bBusy = 0;
                    bInputLocked = 0;
                    return;
                }
                if (pWorld->bHidePlayer == 0)
                    DrawZoneCell(cx, cy);
                int t = (short)pWorld->currentZone->GetTile(tx, ty, 1);
                if (t == -1)
                {
                    int nEdge = pWorld->currentZone->GetEdgeCode(tx, ty);
                    if (nEdge > 0)
                    {
                        switch (nEdge)
                        {
                        case 1:
                            PlaySound(6);
                            pWorld->DrawPlayer();
                            DrawGameArea(NULL);
                            break;
                        case 2:
                        {
                            int px = pWorld->playerX;
                            int py = pWorld->playerY - 1;
                            int nCell = (short)pWorld->GetZoneCell(px, py);
                            if (nCell >= 0)
                            {
                                Zone *pZone = pWorld->GetZoneById(nCell);
                                int nT = (short)pZone->GetTile(tx, pZone->height - 1, 1);
                                if (nT == -1)
                                {
                                    nTransitionStep = 0;
                                    pWorld->currentZone = pZone;
                                    pWorld->RefreshZone();
                                    pWorld->pPlayerFrameTile = (Tile *)pWorld->pPlayerChar->GetFrameTile(nMoveDX, nMoveDY, &pWorld->tiles, nWalkFramePhase);
                                    pWorld->DrawPlayer();
                                    pWorld->cameraX = tx * 32;
                                    pWorld->cameraY = (pZone->height - 1) * 32;
                                    pWorld->UpdateCamera();
                                    pWorld->scrollDirX = 0;
                                    pWorld->scrollDirY = -1;
                                    pWorld->nFrameMode = 6;
                                    pWorld->nMapChangeReason = 5;
                                    pWorld->playerX = px;
                                    pWorld->playerY = py;
                                    if (bKeyboardMoveActive == 0)
                                    {
                                        pWorld->nQueuedMoveDY = 0;
                                        pWorld->nQueuedMoveDX = pWorld->nQueuedMoveDY;
                                        nMoveDX = nMoveDY = pWorld->nQueuedMoveDX;
                                    }
                                    ScrollZoneTransition();
                                }
                                else
                                {
                                    PlaySound(6);
                                }
                            }
                            else
                            {
                                PlaySound(6);
                            }
                            break;
                        }
                        case 3:
                        {
                            int px = pWorld->playerX;
                            int py = pWorld->playerY + 1;
                            int nCell = (short)pWorld->GetZoneCell(px, py);
                            if (nCell >= 0)
                            {
                                Zone *pZone = pWorld->GetZoneById(nCell);
                                int nT = (short)pZone->GetTile(tx, 0, 1);
                                if (nT == -1)
                                {
                                    nTransitionStep = 0;
                                    pWorld->currentZone = pZone;
                                    pWorld->RefreshZone();
                                    pWorld->pPlayerFrameTile = (Tile *)pWorld->pPlayerChar->GetFrameTile(nMoveDX, nMoveDY, &pWorld->tiles, nWalkFramePhase);
                                    pWorld->DrawPlayer();
                                    pWorld->cameraX = tx * 32;
                                    pWorld->cameraY = 0;
                                    pWorld->UpdateCamera();
                                    pWorld->scrollDirX = 0;
                                    pWorld->scrollDirY = 1;
                                    pWorld->nFrameMode = 6;
                                    pWorld->nMapChangeReason = 5;
                                    pWorld->playerX = px;
                                    pWorld->playerY = py;
                                    if (bKeyboardMoveActive == 0)
                                    {
                                        pWorld->nQueuedMoveDY = 0;
                                        pWorld->nQueuedMoveDX = pWorld->nQueuedMoveDY;
                                        nMoveDX = nMoveDY = pWorld->nQueuedMoveDX;
                                    }
                                    ScrollZoneTransition();
                                }
                                else
                                {
                                    PlaySound(6);
                                }
                            }
                            else
                            {
                                PlaySound(6);
                            }
                            break;
                        }
                        case 4:
                        {
                            int py = pWorld->playerY;
                            int px = pWorld->playerX - 1;
                            int nCell = (short)pWorld->GetZoneCell(px, py);
                            if (nCell >= 0)
                            {
                                Zone *pZone = pWorld->GetZoneById(nCell);
                                int nT = (short)pZone->GetTile(pZone->width - 1, ty, 1);
                                if (nT == -1)
                                {
                                    nTransitionStep = 0;
                                    pWorld->currentZone = pZone;
                                    pWorld->RefreshZone();
                                    pWorld->pPlayerFrameTile = (Tile *)pWorld->pPlayerChar->GetFrameTile(nMoveDX, nMoveDY, &pWorld->tiles, nWalkFramePhase);
                                    pWorld->DrawPlayer();
                                    pWorld->cameraX = (pZone->width - 1) * 32;
                                    pWorld->cameraY = ty * 32;
                                    pWorld->UpdateCamera();
                                    pWorld->scrollDirX = -1;
                                    pWorld->scrollDirY = 0;
                                    pWorld->nFrameMode = 6;
                                    pWorld->nMapChangeReason = 5;
                                    pWorld->playerX = px;
                                    pWorld->playerY = py;
                                    if (bKeyboardMoveActive == 0)
                                    {
                                        pWorld->nQueuedMoveDY = 0;
                                        pWorld->nQueuedMoveDX = pWorld->nQueuedMoveDY;
                                        nMoveDX = nMoveDY = pWorld->nQueuedMoveDX;
                                    }
                                    ScrollZoneTransition();
                                    pWorld->scrollDirX = 0;
                                }
                                else
                                {
                                    PlaySound(6);
                                }
                            }
                            else
                            {
                                PlaySound(6);
                            }
                            break;
                        }
                        case 5:
                        {
                            int py = pWorld->playerY;
                            int px = pWorld->playerX + 1;
                            int nCell = (short)pWorld->GetZoneCell(px, py);
                            if (nCell >= 0)
                            {
                                Zone *pZone = pWorld->GetZoneById(nCell);
                                int nT = (short)pZone->GetTile(0, ty, 1);
                                if (nT == -1)
                                {
                                    nTransitionStep = 0;
                                    pWorld->currentZone = pZone;
                                    pWorld->RefreshZone();
                                    pWorld->pPlayerFrameTile = (Tile *)pWorld->pPlayerChar->GetFrameTile(nMoveDX, nMoveDY, &pWorld->tiles, nWalkFramePhase);
                                    pWorld->DrawPlayer();
                                    pWorld->cameraX = 0;
                                    pWorld->cameraY = ty * 32;
                                    pWorld->UpdateCamera();
                                    pWorld->scrollDirX = 1;
                                    pWorld->scrollDirY = 0;
                                    pWorld->nFrameMode = 6;
                                    pWorld->nMapChangeReason = 5;
                                    pWorld->playerX = px;
                                    pWorld->playerY = py;
                                    if (bKeyboardMoveActive == 0)
                                    {
                                        pWorld->nQueuedMoveDY = 0;
                                        pWorld->nQueuedMoveDX = pWorld->nQueuedMoveDY;
                                        nMoveDX = nMoveDY = pWorld->nQueuedMoveDX;
                                    }
                                    ScrollZoneTransition();
                                }
                                else
                                {
                                    PlaySound(6);
                                }
                            }
                            else
                            {
                                PlaySound(6);
                            }
                            break;
                        }
                        default:
                            break;
                        }
                        break;
                    }
                    ZoneObj *pObj = pWorld->currentZone->FindObjectAt(tx, ty);
                    if (pObj == NULL || pObj->type == OBJ_LOCK)
                    {
                        pWorld->cameraX = tx * 32;
                        pWorld->cameraY = ty * 32;
                        pWorld->pPlayerFrameTile = (Tile *)pWorld->pPlayerChar->GetFrameTile(nMoveDX, nMoveDY, &pWorld->tiles, nWalkFramePhase);
                        pWorld->DrawPlayer();
                        pWorld->UpdateCamera();
                        if (bPush)
                        {
                            int ndx = -dx;
                            int ndy = -dy;
                            int sy = cy + ndy;
                            int sx = ndx + cx;
                            int tSide = (short)pWorld->currentZone->GetTile(sx, sy, 1);
                            int tHere = (short)pWorld->currentZone->GetTile(cx, cy, 1);
                            if (tSide >= 0 && tHere < 0 &&
                                (pWorld->GetTileData(tSide)->flags & TILE_PUSH_PULL_BLOCK) != 0)
                            {
                                pWorld->currentZone->SetTile(sx, sy, 1, -1);
                                pWorld->currentZone->SetTile(cx, cy, 1, tSide);
                                PlaySound(1);
                                DrawZoneCell((short)ndx + (short)cx, (short)ndy + (short)cy);
                                DrawZoneCell(cx, cy);
                            }
                        }
                        DrawGameArea(NULL);
                        break;
                    }
                    switch (pObj->type)
                    {
                    case OBJ_QUEST_ITEM_SPOT:
                    case OBJ_THE_FORCE:
                    case OBJ_LOCATOR:
                    case OBJ_ITEM:
                    case OBJ_WEAPON:
                    case OBJ_UNKNOWN:
                        pWorld->pPlayerFrameTile = (Tile *)pWorld->pPlayerChar->GetFrameTile(nMoveDX, nMoveDY, &pWorld->tiles, nWalkFramePhase);
                        pWorld->DrawPlayer();
                        DrawGameArea(NULL);
                        if (pObj->arg >= 0)
                        {
                            nPickupX = tx;
                            nPickupY = ty;
                            nPickupTileId = pObj->arg;
                            pPickupObj = pObj;
                            nTransitionStep = 0;
                            bBlinkState = 0;
                            pWorld->nFrameMode = 9;
                            bMouseCaptured = 0;
                        }
                    default:
                        break;
                    case OBJ_VEHICLE_TO:
                    case OBJ_VEHICLE_FROM:
                        pWorld->cameraX = tx * 32;
                        pWorld->cameraY = ty * 32;
                        pWorld->pPlayerFrameTile = (Tile *)pWorld->pPlayerChar->GetFrameTile(nMoveDX, nMoveDY, &pWorld->tiles, nWalkFramePhase);
                        pWorld->DrawPlayer();
                        pWorld->UpdateCamera();
                        DrawGameArea(NULL);
                        ApplyHotspotCamera(pObj);
                        break;
                    case OBJ_DOOR_IN:
                    case OBJ_DOOR_OUT:
                        pWorld->cameraX = tx * 32;
                        pWorld->cameraY = ty * 32;
                        pWorld->pPlayerFrameTile = (Tile *)pWorld->pPlayerChar->GetFrameTile(nMoveDX, nMoveDY, &pWorld->tiles, nWalkFramePhase);
                        pWorld->DrawPlayer();
                        pWorld->UpdateCamera();
                        DrawGameArea(NULL);
                        TransitionZoneDoor(pObj);
                        break;
                    case OBJ_LOCK:
                        pWorld->cameraX = tx * 32;
                        pWorld->cameraY = ty * 32;
                        pWorld->pPlayerFrameTile = (Tile *)pWorld->pPlayerChar->GetFrameTile(nMoveDX, nMoveDY, &pWorld->tiles, nWalkFramePhase);
                        pWorld->DrawPlayer();
                        pWorld->UpdateCamera();
                        DrawGameArea(NULL);
                        break;
                    case OBJ_TELEPORTER:
                        pWorld->cameraX = tx * 32;
                        pWorld->cameraY = ty * 32;
                        pWorld->pPlayerFrameTile = (Tile *)pWorld->pPlayerChar->GetFrameTile(nMoveDX, nMoveDY, &pWorld->tiles, nWalkFramePhase);
                        pWorld->UpdateCamera();
                        pWorld->DrawPlayer();
                        DrawGameArea(NULL);
                        if (pWorld->inventory.GetSize() > 0 &&
                            (pWorld->inventory.GetAt(0) == NULL ||
                             pWorld->tiles.GetAt(IDX_LOCATOR_ITEM) == (CObject *)((InvItem *)pWorld->inventory.GetAt(0))->pTile))
                        {
                            bMapTeleportEnabled = 1;
                            PlaySound(0x31);
                            CDC *pDC = CDC::FromHandle(::GetDC(m_hWnd));
                            CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
                            pMapReturnZone = pWorld->currentZone;
                            bMapViewOpen = 1;
                            pWorld->DrawLocatorMap(pDC, bBlinkState, bMapTeleportEnabled);
                            pDC->SelectPalette(pOldPal, 0);
                            ::ReleaseDC(m_hWnd, pDC->m_hDC);
                            bMouseCaptured = 0;
                            pWorld->nFrameMode = 7;
                            pWorld->nMapChangeReason = 4;
                        }
                        break;
                    case OBJ_XWING_FROM:
                    case OBJ_XWING_TO:
                        pWorld->cameraX = tx * 32;
                        pWorld->cameraY = ty * 32;
                        pWorld->pPlayerFrameTile = (Tile *)pWorld->pPlayerChar->GetFrameTile(nMoveDX, nMoveDY, &pWorld->tiles, nWalkFramePhase);
                        pWorld->DrawPlayer();
                        pWorld->UpdateCamera();
                        DrawGameArea(NULL);
                        break;
                    }
                    break;
                }
                Tile *pTile = pWorld->GetTileData(t);
                if (pTile == NULL)
                    break;
                pWorld->DrawPlayer();
                UINT nMask = pWorld->currentZone->IactRun(2, cx, cy, dx2, dy2, 0, NULL, pWorld, this);
                if ((pWorld->currentZone->type == 6 || pWorld->currentZone->type == 7 ||
                     pWorld->currentZone->type == 11) && TriggerHotspotsMaybe() == 1)
                    break;
                if ((nMask & 0x2a) != 0)
                {
#ifndef GAME_INDY
                    // Indy's bump handler (DESKADV FUN_1018_733e) has NO persistent text-lock
                    // branch: a bump script's text command shows its dialog SYNCHRONOUSLY inside
                    // IactRun (frame-mode transiently 5), so by here the text is already on screen
                    // and the handler just aborts the move. Yoda instead parks in frame-mode 3 +
                    // bTextDialogShown + bInputLocked and returns — for Indy that extra lock eats
                    // the next user press, so the "home sweet home" text appears one step late and
                    // the door warp needs a back-and-forth. Skip it for Indy; fall through to the
                    // plain move-abort below (matches DESKADV). Yoda #else path = exact original.
                    if ((nMask & 2) != 0 && (nMask & 0x808) == 0)
                    {
                        pWorld->nFrameMode = 3;
                        bMouseCaptured = 0;
                        bTextDialogShown = 1;
                        if (bDialogClickDismissMaybe == 0)
                        {
                            bInputLocked = 1;
                            return;
                        }
                        bInputLocked = 0;
                        return;
                    }
#endif
                    if (pWorld->nFrameMode == 2)
                        pWorld->nFrameMode = 3;
                    pWorld->DrawPlayer();
                    DrawGameArea(NULL);
                    break;
                }
                UINT nFlags = pTile->flags;
                if ((nFlags & TILE_CHARACTER) != 0)
                {
                    if ((pTile->flags & 0x20000) != 0)
                    {
                        short nChar = FindEntityAt(cx + dx2, dy2 + cy);
                        if (nChar < 0)
                            break;
                        Character *pChar = (Character *)pWorld->characters.GetAt(nChar);
                        if (pChar == NULL)
                            break;
                        if (pChar->damage < 0)
                            break;
                        PlaySound(4);
                        AddHealth(-pChar->damage);
                        break;
                    }
                    pWorld->unk3370 = 1;
                    pWorld->equippedItem = pTile;
                    ShowWinMessage(cx, cy, dx2, dy2);
                    break;
                }
                if (bPush && (nFlags & TILE_PUSH_PULL_BLOCK) != 0)
                {
                    int bx = dx + tx;
                    int by = dy + ty;
                    if ((short)pWorld->currentZone->GetTile(bx, by, 1) >= 0)
                        break;
                    if ((short)pWorld->currentZone->GetTile(bx, by, 0) < 0)
                        break;
                    pWorld->currentZone->SetTile(tx, ty, 1, -1);
                    DrawZoneCell(tx, ty);
                    DrawZoneCell(cx, cy);
                    pWorld->currentZone->SetTile(bx, by, 1, t);
                    DrawZoneCell(bx, by);
                    PlaySound(1);
                    UpdateItemObjectsMaybe();
                    ZoneObj *pAt = pWorld->currentZone->FindObjectAt(tx, ty);
                    if (pAt == NULL || pAt->type == OBJ_LOCK)
                    {
                        pWorld->cameraX = tx * 32;
                        pWorld->cameraY = ty * 32;
                    }
                    pWorld->pPlayerFrameTile = (Tile *)pWorld->pPlayerChar->GetFrameTile(nMoveDX, nMoveDY, &pWorld->tiles, nWalkFramePhase);
                    pWorld->DrawPlayer();
                    pWorld->UpdateCamera();
                    DrawGameArea(NULL);
                    break;
                }
                if (bPush)
                    break;
                bPush = 0;
                ZoneObj *pObj2 = pWorld->currentZone->FindObjectAt(tx, ty);
                if (pObj2 != NULL && (pTile->flags & TILE_MIDDLE_LAYER_COLLIDING) == 0)
                {
                    switch (pObj2->type)
                    {
                    case OBJ_QUEST_ITEM_SPOT:
                    case OBJ_THE_FORCE:
                    case OBJ_LOCATOR:
                    case OBJ_ITEM:
                    case OBJ_WEAPON:
                    case OBJ_UNKNOWN:
                        pWorld->pPlayerFrameTile = (Tile *)pWorld->pPlayerChar->GetFrameTile(nMoveDX, nMoveDY, &pWorld->tiles, nWalkFramePhase);
                        pWorld->DrawPlayer();
                        if (pObj2->arg >= 0)
                        {
                            bPush = 1;
                            nPickupX = tx;
                            nPickupY = ty;
                            pWorld->currentZone->SetTile(tx, ty, 1, -1);
                            DrawZoneCell(tx, ty);
                            nPickupTileId = pObj2->arg;
                            pPickupObj = pObj2;
                            nTransitionStep = 0;
                            bBlinkState = 0;
                            pWorld->nFrameMode = 9;
                            bMouseCaptured = 0;
                        }
                        DrawGameArea(NULL);
                    }
                }
                if (bPush || (nMask & 4) != 0)
                    break;
                switch (pWorld->currentZone->IactProbeMove(cx, cy, dx2, dy2, (PTRINT)&pWorld->tiles, 0))
                {
                case 0:
                    UpdatePlayerWalkFrame();
                    break;
                case 1:
                    pWorld->cameraX = tx * 32;
                    pWorld->cameraY = ty * 32;
                    pWorld->pPlayerFrameTile = (Tile *)pWorld->pPlayerChar->GetFrameTile(nMoveDX, nMoveDY, &pWorld->tiles, nWalkFramePhase);
                    pWorld->DrawPlayer();
                    DrawGameArea(NULL);
                    break;
                case 2:
                    dx2 = dx2 + 1;
                    continue;
                case 3:
                    dx2 = dx2 - 1;
                    continue;
                case 4:
                    dy2 = dy2 + 1;
                    continue;
                case 5:
                    dy2 = dy2 - 1;
                    continue;
                default:
                    UpdatePlayerWalkFrame();
                    break;
                }
                break;
            }
            pWorld->nQueuedMoveDY = 0;
            pWorld->nQueuedMoveDX = pWorld->nQueuedMoveDY;
            if (bKeyboardMoveActive == 0)
            {
                nMoveDY = 0;
                nMoveDX = 0;
            }
            if (bKeyboardMoveActive == 0)
            {
                OnMouseMove(bShiftHeld == 0 ? 1 : 5, CPoint(nMouseX, nMouseY));
            }
            OnSetCursor(this, HTCLIENT, 0);
            bBusy = 0;
            bInputLocked = 0;
            return;
        }
        bBusy = 0;
        bInputLocked = 0;
    }
}

// FUNCTION: YODA 0x004150a0
// Recomputes the player's sprite tile for the current facing (nMoveDX,nMoveDY) and repaints.
void CDeskcppView::UpdatePlayerWalkFrame()
{
    bBusy = 1;
    pWorld->pPlayerFrameTile =
        (Tile *)pWorld->pPlayerChar->GetWalkFrameTile(nMoveDX, nMoveDY, &pWorld->tiles);
    pWorld->DrawPlayer();
    DrawGameArea(NULL);
    bBusy = 0;
}

// FUNCTION: YODA 0x004150f0
// WM_KEYDOWN. Space/Insert fire the weapon in the current facing (or in mode 9 collect the
// blinking pickup); VK_PRIOR..VK_DOWN queue a move that commits in the shared tail; 'L' toggles
// the locator map (open needs the locator tile 0x1a5 as the first inventory item; close restores
// the play zone); Ctrl+F8 pops a debug dialog with zone id / camera cell / goal item.
// [EFFECTIVE-WIP: align=452/reg_pen=6, byte_diff 281/1668 (~83% bytes). ALL case bodies match
//  (fire/pickup/locator/debug transcribed 1:1). Two residuals, both parked:
//  (1) SHARED-TAIL PLACEMENT (the dominant cost, block-sinking family — see v8/v9 parks): the
//      original emits the `if(bMoved)` move-commit tail LAST (after case VK_F8, which falls
//      through to it); cl here places it right after the `default:` block (which then falls
//      through to it) and every other case JMPs back. Steering attempts ALL inert: default first
//      / mid / last, VK_F8 with-break vs fall-through, case reorder — cl always makes default the
//      tail's fall-through predecessor. Likely governed by trace/EH-region ordering cl 4.2 does
//      not expose to source shape. (2) The two GetAsyncKeyState `& 0x8000` tests: orig keeps a
//      redundant `movsx eax,ax` before `test ah,0x80`; ours tests AH directly (short local +
//      int local both failed to reproduce / worsened reg_pen). ⭐ CRACK that landed: the tail's
//      OnMouseMove takes `*(CPoint *)&nMouseX` (reinterpret the adjacent nMouseX/nMouseY as a
//      CPoint — LEA &nMouseX + deref); `CPoint(nMouseX,nMouseY)` spills a temp and
//      `*(POINT*)&nMouseX` adds a conversion copy (both worse). Switch is a jump table over
//      keys 0x20..0x77; VK_F8 has no break (falls through, matching the orig's last-case flow).]
void CDeskcppView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    int bMoved = 0;

    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
        bShiftHeld = 1;
    else
        bShiftHeld = 0;

    switch (nChar)
    {
    case VK_SPACE:      // 0x20
    case VK_INSERT:     // 0x2d
        if (pWorld->nFrameMode == 3)
        {
            if (pWorld->currentWeapon != 0)
            {
                if (nMoveDX == 0 || nMoveDY == 0)
                {
                    nFireDirX = nMoveDX;
                    nFireDirY = nMoveDY;
                }
                else if ((nMoveDX < 0 && nMoveDY < 0) || (nMoveDX == 1 && nMoveDY < 0))
                {
                    nFireDirX = 0;
                    nFireDirY = -1;
                }
                else if (nMoveDX < 0 && nMoveDY == 1)
                {
                    nFireDirX = -1;
                    nFireDirY = 0;
                }
                else if (nMoveDX == 1 && nMoveDY == 1)
                {
                    nFireDirX = 1;
                    nFireDirY = 0;
                }

                if (nFireDirX == 0)
                {
                    if (nFireDirY != 0)
                    {
                        if (nFireDirY < 0)
                        {
                            if (pWorld->cameraY < 0x20)
                                break;
                        }
                        else if (nFireDirY > 0)
                        {
                            if ((pWorld->currentZone->height - 2) * 32 < pWorld->cameraY)
                                break;
                        }
                        nFireStep = 0;
                        bFireKeyLatchMaybe = 1;
                        pWorld->nFrameMode = 8;
                    }
                }
                else
                {
                    if (nFireDirX < 0)
                    {
                        if (pWorld->cameraX < 0x20)
                            break;
                    }
                    else if (nFireDirX > 0)
                    {
                        if ((pWorld->currentZone->width - 2) * 32 < pWorld->cameraX)
                            break;
                    }
                    nFireStep = 0;
                    bFireKeyLatchMaybe = 1;
                    pWorld->nFrameMode = 8;
                }
            }
        }
        else if (pWorld->nFrameMode == 9 && nPickupTileId >= 0 && bBusy == 0)
        {
#ifdef YODA_BUGFIX
pickup_item:
#endif
            AddItemToInv((Tile *)pWorld->tiles.GetAt(nPickupTileId));
            short w = pWorld->mapGrid[pWorld->playerY * 10 + pWorld->playerX].cellItemC;
            if (w >= 0 && (int)w == nPickupTileId)
                pWorld->mapGrid[pWorld->playerY * 10 + pWorld->playerX].flagB = 1;
            if (pPickupObj != 0)
            {
                pPickupObj->state = 0;
                pWorld->currentZone->SetTile(nPickupX, nPickupY, 1, -1);
            }
            DrawTileAt(nPickupX, nPickupY, -1);
            DrawGameArea(0);
            pWorld->nFrameMode = 3;
            bMouseCaptured = 0;
        }
        break;
#ifdef YODA_BUGFIX
    // Added: Also pick up items with return key
    case VK_RETURN:
        if (pWorld->nFrameMode == 9 && nPickupTileId >= 0 && bBusy == 0)
        {
            goto pickup_item;
        }
        break;
#endif
    case VK_PRIOR:      // 0x21
    case VK_NEXT:       // 0x22
    case VK_END:        // 0x23
    case VK_HOME:       // 0x24
    case VK_LEFT:       // 0x25
    case VK_UP:         // 0x26
    case VK_RIGHT:      // 0x27
    case VK_DOWN:       // 0x28
        bMoved = 1;
        nMoveCommand = nChar;
        bKeyboardMoveActive = 1;
        break;
    default:
        Default();
        break;
    case 0x4c:          // 'L' — toggle locator map
        if (bLocatorKeyLatchMaybe == 0)
        {
            bLocatorKeyLatchMaybe = 1;
            if (pWorld->nFrameMode == 7 && pWorld->nMapChangeReason == 4)
            {
                PlaySound(0x23);
                pWorld->currentZone = pMapReturnZone;
                bMapViewOpen = 0;
                pWorld->bHidePlayer = 0;
                pWorld->nFrameMode = 3;
                pWorld->RefreshZone();
                pWorld->UpdateCamera();
                DrawGameArea(0);
                bMapTeleportEnabled = 0;
            }
            else if (pWorld->nFrameMode == 3 && bBusy == 0 && pWorld->inventory.GetSize() > 0 &&
                     ((InvItem *)pWorld->inventory.GetAt(0))->pTile == (Tile *)pWorld->tiles.GetAt(IDX_LOCATOR_ITEM))
            {
                CDC *pDC = CDC::FromHandle(::GetDC(m_hWnd));
                CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
                bMapTeleportEnabled = 0;
                bMapViewOpen = 1;
                pMapReturnZone = pWorld->currentZone;
                PlaySound(0x22);
                pWorld->DrawLocatorMap(pDC, bBlinkState, bMapTeleportEnabled);
                pDC->SelectPalette(pOldPal, 0);
                ::ReleaseDC(m_hWnd, pDC->m_hDC);
                pWorld->nFrameMode = 7;
                pWorld->nMapChangeReason = 4;
                bMouseCaptured = 0;
            }
        }
        break;
    case VK_F8:         // 0x77 — Ctrl+F8 debug info dialog
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
        {
            CTextDialog dlg(0);
            char szBuf[20];
            sprintf(szBuf, "%d", pWorld->GetZoneIndex(pWorld->currentZone));
            dlg.m_strField3 = szBuf;
            sprintf(szBuf, "%d", pWorld->cameraX / 32);
            dlg.m_strField1 = szBuf;
            sprintf(szBuf, "%d", pWorld->cameraY / 32);
            dlg.m_strField2 = szBuf;
            sprintf(szBuf, "%d", pWorld->nCurrentGoalItem);
            dlg.m_strField0 = szBuf;
            dlg.DoModal();
            bDebugFlagMaybe = 0;
        }
        // falls through to the shared move-commit tail (last case, no break)
    }

    if (bMoved)
    {
        if (pWorld->nFrameMode == 3)
            pWorld->nFrameMode = 2;
        nMovePending = 1;
        if (bKeyboardMoveActive == 0)
        {
            if (bShiftHeld == 0)
                OnMouseMove(0, *(CPoint *)&nMouseX);
            else
                OnMouseMove(4, *(CPoint *)&nMouseX);
        }
        OnSetCursor(this, HTCLIENT, 0);
    }
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00415820
// GameView::CheckCheat — called after a cheat string is typed into strCheatBuffer
// (built up by OnChar). "goyoda" ⇒ invincibility; "gojedi" ⇒ the full weapon set
// (tiles 0x1ff-0x202) + a Super-Jedi banner. On a hit, strCheatBuffer is reset to "".
// On no match, returns without resetting (the goto skips the reset). The two inline
// strcmps are the /Oi intrinsic (the sbb (1-b)-(b!=0) idiom).
// EFFECTIVE (align 232, was 366 before the per-arm `strCheatBuffer=""; return;`
// copy): the residual is a whole-function register-role swap - the original keeps
// this in esi and reads strCheatBuffer this-relative ([esi+0x17c]), while ours CSEs
// &strCheatBuffer into a callee-saved reg (this -> edi). That reallocation cascades
// (playerX/Y coord LEA-vs-shl+add scheduling). Minimal-TU-probe / G1 dial territory.
// ---------------------------------------------------------------------------
void CDeskcppView::CheckCheat()
{
    CString str = "goyoda";
    if (strcmp(str, strCheatBuffer) == 0)
    {
        str = "Invincible!";
        ShowTextDialog(str, pWorld->playerX * 0x1c + 0x12, pWorld->playerY * 0x1c + 0x12, 1);
        bInvincibleCheat = 1;
        strCheatBuffer = "";
        return;
    }
    str = "gojedi";
    if (strcmp(str, strCheatBuffer) != 0)
        return;
    AddItemToInv(pWorld->GetTileData(0x1ff));
    AddItemToInv(pWorld->GetTileData(0x200));
    AddItemToInv(pWorld->GetTileData(0x201));
    AddItemToInv(pWorld->GetTileData(0x202));
    AddItemToInv(pWorld->GetTileData(0x202));
    AddItemToInv(pWorld->GetTileData(0x202));
    AddItemToInv(pWorld->GetTileData(0x202));
    AddItemToInv(pWorld->GetTileData(0x202));
    str = "Super Jedi!";
    ShowTextDialog(str, pWorld->playerX * 0x1c + 0x12, pWorld->playerY * 0x1c + 0x12, 1);
    strCheatBuffer = "";
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00415a50
// GameView::OnKeyUp (WM_KEYUP): clear the locator-key latch + keyboard-move state,
// re-sample the shift key, and stop a keyboard walk (mode 2 -> 3). Ends with Default().
// EFFECTIVE (align 24, 25/25 insns): the int-local `nShift` reproduces the sign-extend
// but the GetAsyncKeyState test/`xor ecx,ecx` schedule 1-2 slots later than the original
// (the v26 OnKeyDown GetAsyncKeyState `& 0x8000` scheduling family). Parked for G1.
// ---------------------------------------------------------------------------
void CDeskcppView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    bLocatorKeyLatchMaybe = 0;
    int nShift = GetAsyncKeyState(VK_SHIFT);
    bShiftHeld = 1;
    if ((nShift & 0x8000) == 0)
        bShiftHeld = 0;
    int *pMode = &pWorld->nFrameMode;
    bKeyboardMoveActive = 0;
    bDebugFlagMaybe = 0;
    nMovePending = 0;
    nMoveCommand = -1;
    if (*pMode == 2)
    {
        *pMode = 3;
        bMouseCaptured = 0;
    }
    Default();
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00415ac0
// GameView::OnDestroy (WM_DESTROY): kill the frame timer and destroy the inventory
// scroll bar, then chain to the base.
// ---------------------------------------------------------------------------
void CDeskcppView::OnDestroy()
{
    ::KillTimer(m_hWnd, 0x1d1d);
    if (pInvScrollBar != NULL)
        delete pInvScrollBar;
    pInvScrollBar = NULL;
    CView::OnDestroy();
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00415af0
// GameView::CyclePalette — the animated-palette tick (bacta bubbles, fires, water).
// Ring-shifts entry runs of World.sysPalette (the inline-LOGPALETTE PALETTEENTRY[256]
// mirror @0x2e6c) and the DIB color-table mirror pSysColorTable in lockstep, then
// pushes the two animated bands to the Canvas DIB (SetPalette) and the screen palette
// (AnimatePalette + RealizePalette). Every 2nd tick (even nPaletteClock) also runs the
// slower rings/swaps. NO locals for pWorld/the tables: every store goes through
// PALETTEENTRY*/RGBQUAD* lvalues that may alias them, so the compiler reloads
// [this+0x44] / [pWorld+0x326c] per statement — exactly the original shape.
// EFFECTIVE (first compile: 304/304 insns, align=0, 6B): one eax<->ecx role swap in the
// FIRST ::AnimatePalette's setup only — the orig's two IDENTICAL statements get OPPOSITE
// allocations by position (the ZTS<->WES parity-crossing family, not source-steerable). G1.
// ---------------------------------------------------------------------------
void CDeskcppView::CyclePalette()
{
    if (pWorld->bPaletteAnimEnabled == 0)
        return;
#ifdef GAME_INDY
    // Indy animates a different set of palette ranges than Yoda (DESKADV IndyCyclePalette
    // 1018:8e40). Every tick: ring-rotate [160..167], [224..228], [229..237] UP by one.
    // Odd tick only: ring-rotate [238..243] DOWN by one + swap 244<->245. Then push the
    // single animated band [160..245] (count 0x56) to the DIB + screen (Indy has no
    // [10..14] low band). Reusing Yoda's ranges cycled the wrong (brown) colours.
    {
        PALETTEENTRY se;
        RGBQUAD sc;
        int i;
        // ring [160..167] up
        se = pWorld->sysPalette[167]; sc = pWorld->pSysColorTable[167];
        for (i = 167; i > 160; i--) {
            pWorld->sysPalette[i] = pWorld->sysPalette[i - 1];
            pWorld->pSysColorTable[i] = pWorld->pSysColorTable[i - 1];
        }
        pWorld->sysPalette[160] = se; pWorld->pSysColorTable[160] = sc;
        // ring [224..228] up
        se = pWorld->sysPalette[228]; sc = pWorld->pSysColorTable[228];
        for (i = 228; i > 224; i--) {
            pWorld->sysPalette[i] = pWorld->sysPalette[i - 1];
            pWorld->pSysColorTable[i] = pWorld->pSysColorTable[i - 1];
        }
        pWorld->sysPalette[224] = se; pWorld->pSysColorTable[224] = sc;
        // ring [229..237] up
        se = pWorld->sysPalette[237]; sc = pWorld->pSysColorTable[237];
        for (i = 237; i > 229; i--) {
            pWorld->sysPalette[i] = pWorld->sysPalette[i - 1];
            pWorld->pSysColorTable[i] = pWorld->pSysColorTable[i - 1];
        }
        pWorld->sysPalette[229] = se; pWorld->pSysColorTable[229] = sc;
        if ((nPaletteClock++ & 1) != 0) {
            // ring [238..243] DOWN
            se = pWorld->sysPalette[238]; sc = pWorld->pSysColorTable[238];
            for (i = 238; i < 243; i++) {
                pWorld->sysPalette[i] = pWorld->sysPalette[i + 1];
                pWorld->pSysColorTable[i] = pWorld->pSysColorTable[i + 1];
            }
            pWorld->sysPalette[243] = se; pWorld->pSysColorTable[243] = sc;
            // swap 244<->245
            se = pWorld->sysPalette[245];
            pWorld->sysPalette[245] = pWorld->sysPalette[244];
            pWorld->sysPalette[244] = se;
            sc = pWorld->pSysColorTable[245];
            pWorld->pSysColorTable[245] = pWorld->pSysColorTable[244];
            pWorld->pSysColorTable[244] = sc;
        }
        pWorld->pCanvas->SetPalette(0xa0, 0x56, pWorld->pSysColorTable + 0xa0);
        CDC *pDC = CDC::FromHandle(::GetDC(m_hWnd));
        CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
        ::AnimatePalette((HPALETTE)pWorld->pPalette->m_hObject, 0xa0, 0x56,
                         &pWorld->sysPalette[0xa0]);
        ::RealizePalette(pDC->m_hDC);
        pDC->SelectPalette(pOldPal, 0);
        ::ReleaseDC(m_hWnd, pDC->m_hDC);
    }
    return;
#endif
    PALETTEENTRY savedEntry;
    RGBQUAD savedColor;
    int i;
    if ((nPaletteClock++ & 1) == 0)
    {
        // ring [229..237] up by one
        savedEntry = pWorld->sysPalette[237];
        savedColor = pWorld->pSysColorTable[237];
        for (i = 8; i > 0; i--)
        {
            pWorld->sysPalette[229 + i] = pWorld->sysPalette[228 + i];
            pWorld->pSysColorTable[229 + i] = pWorld->pSysColorTable[228 + i];
        }
        pWorld->pSysColorTable[229] = savedColor;
        pWorld->sysPalette[229] = savedEntry;
        // swap 244<->245
        savedEntry = pWorld->sysPalette[245];
        pWorld->sysPalette[245] = pWorld->sysPalette[244];
        pWorld->sysPalette[244] = savedEntry;
        savedColor = pWorld->pSysColorTable[245];
        pWorld->pSysColorTable[245] = pWorld->pSysColorTable[244];
        pWorld->pSysColorTable[244] = savedColor;
        // ring [215..223] up by one
        savedEntry = pWorld->sysPalette[223];
        savedColor = pWorld->pSysColorTable[223];
        for (i = 8; i > 0; i--)
        {
            pWorld->sysPalette[215 + i] = pWorld->sysPalette[214 + i];
            pWorld->pSysColorTable[215 + i] = pWorld->pSysColorTable[214 + i];
        }
        pWorld->pSysColorTable[215] = savedColor;
        pWorld->sysPalette[215] = savedEntry;
        // swap 198<->199
        savedEntry = pWorld->sysPalette[199];
        pWorld->sysPalette[199] = pWorld->sysPalette[198];
        pWorld->sysPalette[198] = savedEntry;
        savedColor = pWorld->pSysColorTable[199];
        pWorld->pSysColorTable[199] = pWorld->pSysColorTable[198];
        pWorld->pSysColorTable[198] = savedColor;
        // swap 200<->201
        savedEntry = pWorld->sysPalette[201];
        pWorld->sysPalette[201] = pWorld->sysPalette[200];
        pWorld->sysPalette[200] = savedEntry;
        savedColor = pWorld->pSysColorTable[201];
        pWorld->pSysColorTable[201] = pWorld->pSysColorTable[200];
        pWorld->pSysColorTable[200] = savedColor;
    }
    // every tick: swaps 202<->203, 204<->205, 206<->207
    savedEntry = pWorld->sysPalette[203];
    pWorld->sysPalette[203] = pWorld->sysPalette[202];
    pWorld->sysPalette[202] = savedEntry;
    savedColor = pWorld->pSysColorTable[203];
    pWorld->pSysColorTable[203] = pWorld->pSysColorTable[202];
    pWorld->pSysColorTable[202] = savedColor;
    savedEntry = pWorld->sysPalette[205];
    pWorld->sysPalette[205] = pWorld->sysPalette[204];
    pWorld->sysPalette[204] = savedEntry;
    savedColor = pWorld->pSysColorTable[205];
    pWorld->pSysColorTable[205] = pWorld->pSysColorTable[204];
    pWorld->pSysColorTable[204] = savedColor;
    savedEntry = pWorld->sysPalette[207];
    pWorld->sysPalette[207] = pWorld->sysPalette[206];
    pWorld->sysPalette[206] = savedEntry;
    savedColor = pWorld->pSysColorTable[207];
    pWorld->pSysColorTable[207] = pWorld->pSysColorTable[206];
    pWorld->pSysColorTable[206] = savedColor;
    // ring [224..228] up by one
    savedEntry = pWorld->sysPalette[228];
    savedColor = pWorld->pSysColorTable[228];
    for (i = 4; i > 0; i--)
    {
        pWorld->sysPalette[224 + i] = pWorld->sysPalette[223 + i];
        pWorld->pSysColorTable[224 + i] = pWorld->pSysColorTable[223 + i];
    }
    pWorld->pSysColorTable[224] = savedColor;
    pWorld->sysPalette[224] = savedEntry;
    // ring [238..243] up by one
    savedEntry = pWorld->sysPalette[243];
    savedColor = pWorld->pSysColorTable[243];
    for (i = 5; i > 0; i--)
    {
        pWorld->sysPalette[238 + i] = pWorld->sysPalette[237 + i];
        pWorld->pSysColorTable[238 + i] = pWorld->pSysColorTable[237 + i];
    }
    pWorld->pSysColorTable[238] = savedColor;
    pWorld->sysPalette[238] = savedEntry;
    // ring [10..15] up by one
    savedEntry = pWorld->sysPalette[15];
    savedColor = pWorld->pSysColorTable[15];
    for (i = 5; i > 0; i--)
    {
        pWorld->sysPalette[10 + i] = pWorld->sysPalette[9 + i];
        pWorld->pSysColorTable[10 + i] = pWorld->pSysColorTable[9 + i];
    }
    pWorld->pSysColorTable[10] = savedColor;
    pWorld->sysPalette[10] = savedEntry;
    // push the two animated bands (10..14 and 0xa0..0xf5) to the DIB + screen palette
    pWorld->pCanvas->SetPalette(0xa, 5, pWorld->pSysColorTable + 10);
    pWorld->pCanvas->SetPalette(0xa0, 0x56, pWorld->pSysColorTable + 0xa0);
    CDC *pDC = CDC::FromHandle(::GetDC(m_hWnd));
    CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
    ::AnimatePalette((HPALETTE)pWorld->pPalette->m_hObject, 0xa, 5, &pWorld->sysPalette[10]);
    ::AnimatePalette((HPALETTE)pWorld->pPalette->m_hObject, 0xa0, 0x56, &pWorld->sysPalette[0xa0]);
    ::RealizePalette(pDC->m_hDC);
    pDC->SelectPalette(pOldPal, 0);
    ::ReleaseDC(m_hWnd, pDC->m_hDC);
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00415ff0
// GameView::OnVScroll (WM_VSCROLL): the vertical inventory scrollbar reflects its scroll
// here; forward to the InvScrollBar (which reuses its horizontal handler), else the base.
// v49: was mis-registered as ON_WM_HSCROLL + mis-named OnHScroll — msgcheck proved the
// original map entry #11 is WM_VSCROLL (0x115) pointing at THIS function (0x415ff0, byte-exact).
// The body (CView::OnHScroll / InvScrollBar::OnHScroll reflection) is the original's, unchanged.
// ---------------------------------------------------------------------------
void CDeskcppView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
    if (pInvScrollBar != NULL)
    {
        pInvScrollBar->OnHScroll(nSBCode, nPos, pScrollBar);
        return;
    }
    CView::OnHScroll(nSBCode, nPos, pScrollBar);
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00416030
// GameView::ConfirmExit — cancel any in-progress drag (mode 4), then AfxMessageBox
// (yes/no). On Yes: force the balloon closed, persist MIDILoad if music is on, resume
// the (suspended) music thread, signal the wave-mix pump to stop, close the document,
// and PostQuitMessage. Near-identical twin of OnAppExit (0x416110).
// EFFECTIVE (align 24, 54/54 insns): the only residual is the AfxGetApp() inline
// (AfxGetModuleState()->m_pCurrentWinApp) scheduling relative to the pMusicThread load
// - the original hoists the AfxGetModuleState call earlier. Scheduling tie-break, G1.
// ---------------------------------------------------------------------------
void CDeskcppView::ConfirmExit()
{
    if (pWorld->nFrameMode == 4)
    {
        bDragActive = 0;
        UpdateDragCursor(1);
        nDragSlot = -1;
        nDragLastScreenY = -1;
        nDragLastScreenX = -1;
        pWorld->nFrameMode = 3;
        DrawText(NULL);
    }
    if (AfxMessageBox(0xe01b, MB_YESNO, 0) == IDYES)
    {
        if (bDialogCloseClicked == 0)
        {
            bDialogCloseClicked = 1;
            pWorld->nFrameMode = 3;
        }
        if (pWorld->nMusicEnabled != 0)
            AfxGetApp()->WriteProfileInt("OPTIONS", "MIDILoad", 1);
        if (pMusicThread != NULL)
            ResumeThread(((CWinThread *)pMusicThread)->m_hThread);
        g_bStopMusicThread = 1;
        SetEvent(g_hWaveMixEvent);
        pWorld->OnCloseDocument();
        PostQuitMessage(0);
    }
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00416110
// GameView::OnAppExit (ID_APP_EXIT) — the menu-driven twin of ConfirmExit (0x416030).
// Identical body: cancel drag, confirm, then shut down.
// EFFECTIVE: shares ConfirmExit's AfxGetApp()-inline scheduling residual (twin body).
// ---------------------------------------------------------------------------
void CDeskcppView::OnAppExit()
{
    if (pWorld->nFrameMode == 4)
    {
        bDragActive = 0;
        UpdateDragCursor(1);
        nDragSlot = -1;
        nDragLastScreenY = -1;
        nDragLastScreenX = -1;
        pWorld->nFrameMode = 3;
        DrawText(NULL);
    }
    if (AfxMessageBox(0xe01b, MB_YESNO, 0) == IDYES)
    {
        if (bDialogCloseClicked == 0)
        {
            bDialogCloseClicked = 1;
            pWorld->nFrameMode = 3;
        }
        if (pWorld->nMusicEnabled != 0)
            AfxGetApp()->WriteProfileInt("OPTIONS", "MIDILoad", 1);
        if (pMusicThread != NULL)
            ResumeThread(((CWinThread *)pMusicThread)->m_hThread);
        g_bStopMusicThread = 1;
        SetEvent(g_hWaveMixEvent);
        pWorld->OnCloseDocument();
        PostQuitMessage(0);
    }
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00416120
// GameView::OnCmdDifficulty (WM_COMMAND 0x8005): pause the game (nFrameMode 0), run
// the difficulty slider dialog seeded from World.difficulty, and (on OK) read the new
// value back. bBusy is held across DoModal; the frame mode is restored afterward.
// EXACT since the slider DoDataExchange decls landed (was EFFECTIVE align 0, 9B: a pure
// this<->nSavedMode esi/edi allocator swap under the pre-DDX dial — the dial fixed it,
// vindicating the fixed-point rule).
// ---------------------------------------------------------------------------
void CDeskcppView::OnCmdDifficulty()
{
    DifficultyDlg dlg(this);
    bBusy = 1;
    int nSavedMode = pWorld->nFrameMode;
    pWorld->nFrameMode = 0;
    dlg.m_nValue = pWorld->difficulty;
    if (dlg.DoModal() == 1)
        pWorld->difficulty = dlg.m_nValue;
    pWorld->nFrameMode = nSavedMode;
    bBusy = 0;
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00416220
// GameView::OnTogglePause (WM_COMMAND 0x8002): toggle mode <-> 0xe (paused). On pause,
// bank the elapsed time into timeOffset; on resume, restore the zone/camera and reset
// timeBase so the game clock continues.
// ---------------------------------------------------------------------------
void CDeskcppView::OnTogglePause()
{
    int nMode = pWorld->nFrameMode;
    int *pMode = &pWorld->nFrameMode;
    if (nMode != 0xe)
    {
        nSavedFrameMode = nMode;
        *pMode = 0xe;
        pWorld->timeOffset += (int)difftime(pWorld->timeBase, time(NULL));
        return;
    }
    *pMode = nSavedFrameMode;
    nSavedFrameMode = 3;
    bPauseOverlayDrawn = 0;
    pWorld->RefreshZone();
    pWorld->UpdateCamera();
    DrawGameArea(NULL);
    pWorld->timeBase = (int)time(NULL);
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x004162a0
// GameView::OnUpdatePauseUi (ON_UPDATE_COMMAND_UI 0x8002): enable the Pause item unless
// mid-transition (modes 1/4/5/6/7/9); check it while paused (mode 0).
// ---------------------------------------------------------------------------
void CDeskcppView::OnUpdatePauseUi(CCmdUI *pCmdUI)
{
    switch (pWorld->nFrameMode)
    {
    case 1:
    case 4:
    case 5:
    case 6:
    case 7:
    case 9:
        pCmdUI->Enable(0);
        break;
    default:
        pCmdUI->Enable(1);
    }
    pCmdUI->SetCheck(pWorld->nFrameMode == 0);
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00416310
// GameView::OnCmdGameSpeed (WM_COMMAND 0x800c): run the game-speed slider dialog. The
// slider edits the *inverted* speed (0xba - nGameSpeed, clamped 1..0x5a); on OK the
// value is un-inverted (clamped 0x60..0xb9) into nGameSpeed, mirrored to World.gameSpeed,
// and the frame timer is restarted at the new interval.
// ---------------------------------------------------------------------------
void CDeskcppView::OnCmdGameSpeed()
{
    GameSpeedDlg dlg(this);
    dlg.m_nValue = 0xba - nGameSpeed;
    if (dlg.m_nValue < 1)
        dlg.m_nValue = 1;
    if (dlg.m_nValue > 0x5a)
        dlg.m_nValue = 0x5a;
    if (dlg.DoModal() == 1)
    {
        int nSpeed = 0xba - dlg.m_nValue;
        nGameSpeed = nSpeed;
        if (nSpeed > 0xb9)
            nGameSpeed = 0xb9;
        if ((int)nGameSpeed < 0x60)
            nGameSpeed = 0x60;
        pWorld->gameSpeed = nGameSpeed;
        ::KillTimer(m_hWnd, 0x1d1d);
        nTimerId = ::SetTimer(m_hWnd, 0x1d1d, nGameSpeed, NULL);
    }
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00416460
// GameView::OnUpdateGameSpeedUi (ON_UPDATE_COMMAND_UI 0x800c): disable mid-transition
// (modes 1/4/5/6/8/0xb), else enable only when not busy.
// ---------------------------------------------------------------------------
void CDeskcppView::OnUpdateGameSpeedUi(CCmdUI *pCmdUI)
{
    switch (pWorld->nFrameMode)
    {
    case 1:
    case 4:
    case 5:
    case 6:
    case 8:
    case 0xb:
        pCmdUI->Enable(0);
        return;
    }
    if (bBusy == 0)
        pCmdUI->Enable(1);
    else
        pCmdUI->Enable(0);
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x004164d0
// GameView::OnCmdWorldSizeMaybe (WM_COMMAND 0x800d): run the world-size slider dialog
// seeded from World.worldSize; read it back on OK. Demo-disabled via the UI handler.
// ---------------------------------------------------------------------------
void CDeskcppView::OnCmdWorldSizeMaybe()
{
    WorldSizeDlg dlg(this);
    dlg.m_nValue = pWorld->worldSize;
    if (dlg.DoModal() == 1)
        pWorld->worldSize = dlg.m_nValue;
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x004165a0
// GameView::OnUpdateWorldSizeUi (ON_UPDATE_COMMAND_UI 0x800d): always disabled (demo).
// EFFECTIVE (6B, 13/13): the original materializes pCmdUI in eax then `mov ecx,eax`
// for the vcall (the unused GameView `this` stays in ecx a beat longer); ours loads
// pCmdUI straight to ecx. Allocation artifact of a `this`-ignoring member. G1.
// ---------------------------------------------------------------------------
void CDeskcppView::OnUpdateWorldSizeUi(CCmdUI *pCmdUI)
{
#ifdef YODA_FULL
    pCmdUI->Enable(1);        // full: World Size is selectable
#else
    pCmdUI->Enable(0);
#endif
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x004165b0
// GameView::OnUpdateDifficultyUi (ON_UPDATE_COMMAND_UI 0x8005): same gate as GameSpeed.
// ---------------------------------------------------------------------------
void CDeskcppView::OnUpdateDifficultyUi(CCmdUI *pCmdUI)
{
    switch (pWorld->nFrameMode)
    {
    case 1:
    case 4:
    case 5:
    case 6:
    case 8:
    case 0xb:
        pCmdUI->Enable(0);
        return;
    }
    if (bBusy == 0)
        pCmdUI->Enable(1);
    else
        pCmdUI->Enable(0);
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00416620
// GameView::OnCmdStats (WM_COMMAND 0x800e): run the stats dialog seeded with the four
// score counters, formatted through ONE reused CString temp (Format "%ld" + operator=
// per field, in DDX-ctrl order 0x95/0x96/0x97/0x98's memory order 2,3,1,0). DoModal
// result ignored (display-only). The stack StatsDlg makes this TU emit ??1StatsDlg —
// the binary's copy is at 0x416750 (marker on the class dtor comes free).
// ---------------------------------------------------------------------------
void CDeskcppView::OnCmdStats()
{
    StatsDlg dlg(this, pWorld);
    CString str;
    str.Format("%ld", pWorld->highScore);
    dlg.m_str2 = str;
    str.Format("%ld", pWorld->lastScore);
    dlg.m_str3 = str;
    str.Format("%ld", pWorld->completionCount);
    dlg.m_str1 = str;
    str.Format("%ld", pWorld->lastCount);
    dlg.m_str0 = str;
    dlg.DoModal();
}
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00416800
// GameView::OnUpdateStatsUi (ON_UPDATE_COMMAND_UI 0x800e): always disabled (demo).
// EFFECTIVE (6B, 13/13): same unused-`this` eax-hop as OnUpdateWorldSizeUi. G1.
// ---------------------------------------------------------------------------
void CDeskcppView::OnUpdateStatsUi(CCmdUI *pCmdUI)
{
#ifdef YODA_FULL
    pCmdUI->Enable(1);        // full: Stats is available
#else
    pCmdUI->Enable(0);
#endif
}

// ===========================================================================
// StatsDlg (0x416810-0x416a40) — the demo stats dialog. ClassWizard CDialog with 4
// CString DDX fields; OnCmdStatsMaybe (0x416620, deferred) fills them.
// ===========================================================================

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00416810
// StatsDlg::StatsDlg — CDialog(template 0xe1); store the doc pointer and empty the 4
// DDX strings (the //{{AFX_DATA_INIT block).
// ---------------------------------------------------------------------------
StatsDlg::StatsDlg(CWnd *pParent, CDeskcppDoc *pDoc) : CDialog(0xe1, pParent)
{
    pWorld = pDoc;
    //{{AFX_DATA_INIT(StatsDlg)
    m_str0 = "";
    m_str1 = "";
    m_str2 = "";
    m_str3 = "";
    //}}AFX_DATA_INIT
}

// FUNCTION: YODA 0x00416750  (??1StatsDlg@@UAE@XZ — the plain dtor, emitted for OnCmdStats's
// STACK StatsDlg; EH-framed, destroys the 4 CStrings via per-member funclets then ~CDialog.
// Sits between OnCmdStats 0x416620 and OnUpdateStatsUi 0x416800 in .text.)

// FUNCTION: YODA 0x00416920  (??_GStatsDlg@@UAEPAXI@Z — compiler-emitted scalar dtor, 188B:
// EH-framed body destroying the 4 CString members + handler thunk + funclet. The ctor's own
// EH-handler thunk sits at 0x416902, inside the ctor COMDAT's 260B extent.)

// FUNCTION: YODA 0x004169e0
void StatsDlg::DoDataExchange(CDataExchange *pDX)
{
    //{{AFX_DATA_MAP(StatsDlg)
    DDX_Text(pDX, 0x98, m_str0);
    DDX_Text(pDX, 0x97, m_str1);
    DDX_Text(pDX, 0x95, m_str2);
    DDX_Text(pDX, 0x96, m_str3);
    //}}AFX_DATA_MAP
}

// FUNCTION: YODA 0x00416a30  (?GetMessageMap@StatsDlg@@MBEPBUAFX_MSGMAP@@XZ — empty msgmap
// @0x44b558)
BEGIN_MESSAGE_MAP(StatsDlg, CDialog)
    //{{AFX_MSG_MAP(StatsDlg)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

// FUNCTION: YODA 0x00416a40
BOOL StatsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    CenterWindow();
    return TRUE;
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00416a60
// GameView::OnDialogCloseBtn (BN 0x1389): the in-game text balloon's CLOSE button
// just sets the "close was clicked" flag; the frame loop tears the dialog down.
// ---------------------------------------------------------------------------
void CDeskcppView::OnDialogCloseBtn()
{
    bDialogCloseClicked = 1;
}

// FUNCTION: YODA 0x00416a70
void CDeskcppView::OnDialogUpBtnNop()
{
}

// FUNCTION: YODA 0x00416a80
void CDeskcppView::OnDialogDownBtnNop()
{
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00416a90
// GameView::OnCtlColor (WM_CTLCOLOR): give the balloon's edit control (ctl type 1 =
// CTLCOLOR_EDIT) a WHITE background BRUSH and a white text-BACKGROUND (SetBkColor), leaving
// the text color at its default (black) so it renders visibly; everything else defers.
// Byte-EXACT. NOTE (v33 bugfix): this was `SetTextColor(0xffffff)` (white-on-white =
// invisible bubble text) — the original calls CDC::SetBkColor, at vtable slot +0x34, one
// slot BEFORE SetTextColor (+0x38). The disasm `CALL [EAX+0x34]` pins it to SetBkColor; the
// old "DIFF 1 benign byte" was that vtable-slot displacement — a real semantic bug, not noise.
// ---------------------------------------------------------------------------
HBRUSH CDeskcppView::OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor)
{
    HBRUSH hBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);
    if (nCtlColor == 1)
    {
        pDC->SetBkColor(0xffffff);
        return hBrush;
    }
    return CView::OnCtlColor(pDC, pWnd, nCtlColor);
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00416ae0
// GameView::OnChar (WM_CHAR): while a text balloon is up in the intro zone
// (mode 7 / reason 4), accumulate the cheat-code letters (only the chars used by
// "goyoda"/"gojedi") into strCheatBuffer, then run CheckCheat.
// ---------------------------------------------------------------------------
void CDeskcppView::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (pWorld->nFrameMode == 7 && pWorld->nMapChangeReason == 4)
    {
        switch (nChar)
        {
        case 'a': strCheatBuffer += 'a'; break;
        case 'd': strCheatBuffer += 'd'; break;
        case 'e': strCheatBuffer += 'e'; break;
        case 'g': strCheatBuffer += 'g'; break;
        case 'i': strCheatBuffer += 'i'; break;
        case 'j': strCheatBuffer += 'j'; break;
        case 'o': strCheatBuffer += 'o'; break;
        case 'y': strCheatBuffer += 'y'; break;
        }
    }
    CheckCheat();
    Default();
}

// ===========================================================================
// TextDialog cluster (0x416b90-0x4186e0) — the in-game speech balloon (plain class,
// sizeof 0xc8; see GameView.h). ShowTextDialog (doc TU) news it, seeds the args + text,
// and calls Run(). Layout/Run (0x4176f0/0x416c40) transcribed below the helpers.
// ===========================================================================

// FUNCTION: YODA 0x00416b90
// TextDialog::TextDialog — store the parent view (twice: pView2 + pParentView), clear the
// scalar state and empty the text. unk08 = -1.
TextDialog::TextDialog(CDeskcppView *pView)
{
    pParentView = pView;
    unk00 = 0;
    unk0c = 0;
    soundSession = 0;
    unk08 = -1;
    unk04 = 0;
    strText = "";
    pView2 = pView;
    nMode = 0;
    bAtBottom = 0;
}

// FUNCTION: YODA 0x00416c40
// TextDialog::Run — show the balloon and drive its own modal message pump until the player
// dismisses it (close button / Enter / Esc / click-off). Sets up the font + line metrics,
// sizes the box, positions it (Position), then loops on GetMessage handling keys (up/down
// scroll, Enter/Esc close), timer auto-repeat, and mouse hit-testing against the four rects.
// On exit it repaints the zone (or the locator map, in screen mode) and clears bBusy.
// EFFECTIVE (622/622 insns — structure exact, incl. the < 0x101 / < 0x112 message-range
// ladders, the WM_KEYDOWN wParam switch, and the WM_LBUTTONDOWN 4-rect PtInRect nest with
// its shared `dispatch:` / `rbtn:` cross-jumps): residual is the this-register landing in
// ESI where the original keeps it in EDI (a whole-function allocator tie-break — the same
// esi/edi this-swap family as OnCmdDifficulty/OnBumpTile) plus the scheduling ripple it
// causes. Not source-steerable; G1.
int TextDialog::Run()
{
    pParentView->bBusy = 1;
    pParentView->wndDialogText.ShowWindow(0);
    pParentView->btnDialogClose.ShowWindow(0);
    pParentView->btnDialogDown.ShowWindow(0);
    pParentView->btnDialogUp.ShowWindow(0);
    ::SendMessage(pParentView->wndDialogText.m_hWnd, WM_SETREDRAW, 0, 0);
    ::SendMessage(pParentView->btnDialogClose.m_hWnd, WM_SETREDRAW, 0, 0);
    ::SendMessage(pParentView->btnDialogDown.m_hWnd, WM_SETREDRAW, 0, 0);
    ::SendMessage(pParentView->btnDialogUp.m_hWnd, WM_SETREDRAW, 0, 0);
    HDC hdc = pParentView->pWorld->pCanvas->hdc;
    HFONT h = CreateFont(-8, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, g_pszDialogFont);
    SelectObject(hdc, h);
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    nLineHeight = tm.tmHeight;
    nCharWidth = tm.tmAveCharWidth;
    pParentView->wndDialogText.SetWindowText(strText);
    LRESULT nLines = ::SendMessage(pParentView->wndDialogText.m_hWnd, EM_GETLINECOUNT, 0, 0);
    nTotalLines = nLines;
    nVisibleLines = nLines;
    bTimerActive = 0;
    if (5 < nLines)
    {
        bTimerActive = 1;
        nScrollLine = 5;
        nVisibleLines = 5;
    }
    int vl = nVisibleLines;
    if (vl == 1)
        nTextH = nLineHeight + 1;
    else
    {
        if (vl < 5)
            vl = nLineHeight * vl;
        else
            vl = nLineHeight * 5;
        nTextH = vl;
    }
    nBoxH = nTextH + 10;
    int tw = nCharWidth * 0x1a;
    nTextW = tw;
    unk24 = 0;
    nBoxW = tw + 0x28;
    Position();
    pParentView->bDialogCloseClicked = 0;
    if (pParentView->bShowEmptyDialogOnceMaybe == 1)
        pParentView->bDialogCloseClicked = 1;
    CDeskcppView *pGV = pParentView;
    RECT rc;
    rc.top = rectBox.top - pGV->pWorld->nViewTop;
    rc.left = rectBox.left - pGV->pWorld->nViewLeft;
    rc.right = nBoxW + rc.left;
    rc.bottom = nBoxH + rc.top;
    int done = pGV->bDialogCloseClicked;
    while (done == 0)
    {
        MSG msg;
        POINT pt;
        if (GetMessage(&msg, 0, 0, 0) < 1)
        {
            pParentView->bBusy = 0;
            return 0;
        }
        if (msg.message < 0x101)
        {
            if (msg.message != WM_KEYDOWN)
            {
                if (msg.message != WM_NCLBUTTONDOWN || msg.wParam != VK_MENU)
                    goto dispatch;
                MessageBeep((UINT)-1);
                goto after;
            }
            switch (msg.wParam)
            {
            case VK_RETURN:
            case VK_ESCAPE:
#ifdef YODA_BUGFIX
            case VK_SPACE:
            case VK_SHIFT:
#endif
                pParentView->bDialogClickDismissMaybe = 1;
                pParentView->bDialogCloseClicked = 1;
                break;
            default:
                goto dispatch;
            case VK_UP:
                DispatchMessage(&msg);
                ScrollTextLine2();
                if (bTimerActive != 0)
                    SetTimer(pParentView->m_hWnd, 1, 100, 0);
                break;
            case VK_DOWN:
                DispatchMessage(&msg);
                ScrollTextLine();
                if (bTimerActive != 0)
                    SetTimer(pParentView->m_hWnd, 1, 100, 0);
            }
            goto after;
        }
        if (msg.message < 0x112)
        {
            if (msg.message == WM_COMMAND)
            {
                if ((short)msg.wParam != (short)0xe141)
                    goto dispatch;
            }
            else
            {
                if (msg.message != WM_KEYUP)
                    goto dispatch;
                DispatchMessage(&msg);
                if (bTimerActive != 0)
                    KillTimer(pParentView->m_hWnd, 1);
            }
            goto after;
        }
        switch (msg.message)
        {
        case WM_TIMER:
            if (msg.wParam == 1)
                UpdateDialogButtons(1);
            break;
        default:
            goto dispatch;
        case WM_LBUTTONDOWN:
            pParentView->bDialogClickDismissMaybe = 0;
            pParentView->bBlockBumpUntilClick = 1;
            pt.x = msg.pt.x;
            pt.y = msg.pt.y;
            ScreenToClient(pParentView->m_hWnd, &pt);
            if (PtInRect(&rc, pt) == 0 && nMode != 0)
            {
                if (bTimerActive == 0)
                    pParentView->bDialogCloseClicked = 1;
                else if (bAtBottom != 0)
                    pParentView->bDialogCloseClicked = 1;
            }
            else if (PtInRect(&rectClose, pt) != 0)
            {
                if (bTimerActive == 0)
                    pParentView->bDialogCloseClicked = 1;
                else if (bAtBottom != 0)
                    pParentView->bDialogCloseClicked = 1;
            }
            else if (PtInRect(&rectDown, pt) != 0)
            {
                DispatchMessage(&msg);
                ScrollTextLine();
                if (bTimerActive != 0)
                    SetTimer(pParentView->m_hWnd, 1, 100, 0);
            }
            else if (PtInRect(&rectUp, pt) != 0)
            {
                DispatchMessage(&msg);
                ScrollTextLine2();
                if (bTimerActive != 0)
                    SetTimer(pParentView->m_hWnd, 1, 100, 0);
            }
            else if (PtInRect(&rectText, pt) == 0)
            {
                goto rbtn;
            }
            break;
        case WM_LBUTTONUP:
            DispatchMessage(&msg);
            if (bTimerActive != 0)
                KillTimer(pParentView->m_hWnd, 1);
            break;
        case WM_LBUTTONDBLCLK:
        rbtn:
            pt.x = msg.pt.x;
            pt.y = msg.pt.y;
            ScreenToClient(pParentView->m_hWnd, &pt);
            if (PtInRect(&rectText, pt) == 0)
                goto dispatch;
            break;
        case WM_RBUTTONDOWN:
            pt.x = msg.pt.x;
            pt.y = msg.pt.y;
            ScreenToClient(pParentView->m_hWnd, &pt);
            if (PtInRect(&rc, pt) == 0)
            {
            dispatch:
                DispatchMessage(&msg);
            }
        }
    after:
        done = pParentView->bDialogCloseClicked;
    }
    if (bTimerActive != 0)
        KillTimer(pParentView->m_hWnd, 1);
    ::SendMessage(pParentView->wndDialogText.m_hWnd, WM_SETREDRAW, 0, 0);
    ::SendMessage(pParentView->btnDialogClose.m_hWnd, WM_SETREDRAW, 0, 0);
    ::SendMessage(pParentView->btnDialogDown.m_hWnd, WM_SETREDRAW, 0, 0);
    ::SendMessage(pParentView->btnDialogUp.m_hWnd, WM_SETREDRAW, 0, 0);
    pParentView->wndDialogText.ShowWindow(0);
    pParentView->btnDialogClose.ShowWindow(0);
    pParentView->btnDialogDown.ShowWindow(0);
    pParentView->btnDialogUp.ShowWindow(0);
    if (nMode == 0)
    {
        pParentView->DrawWholeZone();
        pParentView->pWorld->DrawPlayer();
        pParentView->DrawGameArea(0);
        pParentView->bShowEmptyDialogOnceMaybe = 0;
        if (nMode == 0)
            goto done_paint;
    }
    {
        HDC hdc2 = GetDC(pParentView->m_hWnd);
        CDC *pDC = CDC::FromHandle(hdc2);
        CPalette *pOld = pDC->SelectPalette(pParentView->pWorld->pPalette, 0);
        pParentView->pWorld->DrawLocatorMap(pDC, 0, pParentView->bMapTeleportEnabled);
        pDC->SelectPalette(pOld, 0);
        ReleaseDC(pParentView->m_hWnd, pDC->m_hDC);
    }
done_paint:
    bTimerActive = 0;
    pParentView->bDialogCloseClicked = 0;
    pParentView->bBusy = 0;
    return 1;
}

// FUNCTION: YODA 0x00417570
// TextDialog::Position — clamp the requested anchor (nArgX,nArgY) so the bubble stays on
// screen, decide whether its tail points up (2) or down (1), then hand the final top-left to
// Layout. Two coordinate regimes: world-relative (nMode==0, big zone) subtracts the view
// origin; screen-relative (nMode!=0 or a <10-wide zone) works in raw client pixels.
// EFFECTIVE (align 152, 121/120 insns): the shared `goto do_layout` + tail-call to Layout
// fixed the structure; residuals are the cmp-direction family (lesson #6 — several jl/jge vs
// jle flips the C source can't steer; two clamp inversions helped, the nViewRight one didn't)
// plus one edx/ebx/esi allocator rotation. G1.
void TextDialog::Position()
{
    int halfW = nBoxW / 2;
    CDeskcppDoc *pW = pParentView->pWorld;
    int x;
    if (pW->currentZone->width < 10 || nMode != 0)
    {
        int ax = nArgX;
        x = ax - halfW;
        if (x < 0x10)
        {
            x = 6;
            if (ax < 0x20)
                nBoxX = 0x10;
            else
                nBoxX = ax;
        }
        else
        {
            if (ax + halfW < 0x11d || (x = 0x11c - nBoxW, ax < 0x101))
                nBoxX = ax;
            else
                nBoxX = 0x10c;
        }
    }
    else
    {
        int ax = nArgX;
        if ((ax - pW->nViewLeft) - halfW < 0x10)
        {
            x = pW->nViewLeft + 6;
            if (ax < 0x20)
                nBoxX = 0x10;
            else
                nBoxX = ax;
        }
        else if (nMode == 0)
        {
            if ((pW->nViewRight - ax) - halfW < 6)
            {
                x = (pW->nViewRight - nBoxW) - 6;
                if (0x220 < ax)
                    nBoxX = 0x230;
                else
                    nBoxX = ax;
            }
            else
            {
                nBoxX = ax;
                x = ax - halfW;
            }
        }
        else if (ax + halfW < 0x11d)
        {
            nBoxX = ax;
            x = ax - halfW;
        }
        else
        {
            x = 0x11c - nBoxW;
            if (0x100 < ax)
                nBoxX = 0x10c;
            else
                nBoxX = ax;
        }
    }
    int y;
    if (nMode == 0)
    {
        int ay = nArgY;
        if (nBoxH + 0x1e <= ay - pParentView->pWorld->nViewTop)
        {
            nTailDir = 1;
            y = (ay - nBoxH) - 0x1e;
            goto do_layout;
        }
        y = ay + 0x1e;
    }
    else
    {
        int ay = nArgY;
        if (nBoxH + 0x1e <= ay)
        {
            nTailDir = 1;
            y = (ay - nBoxH) - 0x1e;
            goto do_layout;
        }
        if (ay == 0)
            ay = 0x22;
        y = ay;
    }
    nTailDir = 2;
do_layout:
    Layout(x, y);
}

// The speech-bubble tail triangle is built as a 3-point array. The game's point type has an
// OUT-OF-LINE empty default ctor (0x004186e0, `mov eax,ecx; ret`) — NOT MFC's inline CPoint —
// so the array construction in Layout emits three ctor calls (0x417857 loop). Derived from
// tagPOINT so it is LPPOINT-compatible for ::Polygon and its .x/.y feed MoveTo/LineTo.
struct TriPoint : public tagPOINT
{
    TriPoint();
};

// FUNCTION: YODA 0x004176f0
// TextDialog::Layout(x,y) — paint the speech balloon at (x,y): select the dialog font, fill the
// bubble RECTs, RoundRect the frame, MoveWindow the child CEdit, then draw the tail triangle
// (Polygon fill + a white-pen MoveTo/LineTo along the box edge, restored to black pen) and lay
// out + show/hide the three CBitmapButtons (close/up/down) per the visible-line count.
// EFFECTIVE (1419B, align 374, 407/405 insns — structure faithful): three residual families,
// all whole-function allocator/scheduling artifacts, not source-steerable (G1):
//   (a) cl's TRACE-DRIVEN DUPLICATION of the bx-range ladder — the original threads a dead
//       `cmp bx,0x20; jl; cmp bx,0x100` fragment into the low-x branch AND both nTailDir arms
//       (3 copies of a range test whose result is unused). Clean source emits none; the nested
//       vs `else if` form only shuffles which arms cl merges (probed: nested 392 > else-if 374).
//   (b) the two CDCs: the original keeps &rectText.top (esi+0xa0) and &nBoxX (esi+0x18) in
//       POINTER regs (ebx/edx) and RELOADS the members (weak alias analysis vs the local point[]
//       stores), where ours caches the values — a reload-vs-register tie-break (lesson #19).
//   (c) the rectClose/Up/Down store scheduling + this landing in ESI. G1.
// NOTE 0x004186e0 = TriPoint::TriPoint (this TU's last function, EXACT) — the array ctor.
void TextDialog::Layout(int x, int y)
{
    HDC hdc = pParentView->pWorld->pCanvas->hdc;
    HFONT h = CreateFont(-8, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, g_pszDialogFont);
    SelectObject(hdc, h);
    rectBox.left = x;
    rectBox.top = y;
    rectBox.right = nBoxW + x;
    rectBox.bottom = nBoxH + y;
    if (nMode == 0)
    {
        rectText.left = (x - pParentView->pWorld->nViewLeft) + 0xf;
        y -= pParentView->pWorld->nViewTop;
    }
    else
    {
        rectText.left = x + 0xf;
    }
    rectText.top = y + 0xc;
    rectText.right = rectText.left + nTextW;
    rectText.bottom = rectText.top + nTextH;

    CDC *pDC = CDC::FromHandle(GetDC(pParentView->m_hWnd));
    ::RoundRect(hdc, rectBox.left, rectBox.top, rectBox.right, rectBox.bottom, 0x10, 0x10);
    ReleaseDC(pParentView->m_hWnd, pDC->m_hDC);
    pParentView->wndDialogText.MoveWindow(rectText.left, rectText.top, nTextW, nTextH, TRUE);
    nTotalLines = ::SendMessage(pParentView->wndDialogText.m_hWnd, EM_GETLINECOUNT, 0, 0);

    TriPoint point[3];
    int bx = nBoxX;
    if (nMode == 0)
        bx -= pParentView->pWorld->nViewLeft;
    if (bx < 0x90)
    {
        point[1].x = nBoxX;
        point[0].x = nBoxX;
        point[2].x = nBoxX + 0x10;
    }
    else if (bx < 0x101)
    {
        point[1].x = nBoxX;
        point[0].x = nBoxX;
        point[2].x = nBoxX - 0x10;
    }
    else
    {
        point[1].x = nBoxX;
        point[0].x = nBoxX;
        point[2].x = nBoxX - 0x10;
    }
    if (nTailDir == 1)
    {
        point[0].y = rectBox.bottom - 1;
        point[1].y = rectBox.bottom + 0xf;
        point[2].y = rectBox.bottom - 1;
    }
    else if (nTailDir == 2)
    {
        point[0].y = rectBox.top;
        point[1].y = rectBox.top - 0x10;
        point[2].y = rectBox.top;
    }
    ::Polygon(hdc, point, 3);

    pDC = CDC::FromHandle(hdc);
    pDC->SelectStockObject(WHITE_PEN);
    if (nTailDir == 2)
    {
        if (point[2].x < point[0].x)
        {
            pDC->MoveTo(point[2].x + 1, point[2].y);
            pDC->LineTo(point[0].x, point[0].y);
        }
        else
        {
            pDC->MoveTo(point[0].x, point[0].y);
            pDC->LineTo(point[2].x, point[2].y);
        }
    }
    else
    {
        if (point[2].x < point[0].x)
        {
            pDC->MoveTo(point[2].x + 1, point[2].y);
            pDC->LineTo(point[0].x, point[0].y);
        }
        else
        {
            pDC->MoveTo(point[0].x, point[0].y);
            pDC->LineTo(point[2].x, point[2].y);
        }
    }
    pDC->SelectStockObject(BLACK_PEN);

    int t = rectText.bottom - 0xf;
    int l = (rectText.left + nTextW) + 0xc;
    rectClose.left = l;
    rectClose.top = t;
    rectClose.right = l + 0x10;
    rectClose.bottom = t + 0x10;
    int dtop = (t - nLineHeight) - 4;
    rectDown.top = dtop;
    rectDown.left = l;
    rectDown.bottom = t - 4;
    rectDown.right = l + 0x10;
    rectUp.left = l;
    rectUp.right = l + 0x10;
    rectUp.top = (dtop - nLineHeight) - 4;
    rectUp.bottom = dtop - 4;

    switch (nVisibleLines)
    {
    case 1:
        pParentView->btnDialogClose.SetWindowPos(pParentView, l, t, 0x10, 0x10, 4);
        pParentView->btnDialogClose.EnableWindow(1);
        pParentView->btnDialogDown.ShowWindow(0);
        pParentView->btnDialogDown.EnableWindow(0);
        pParentView->btnDialogUp.ShowWindow(0);
        pParentView->btnDialogUp.EnableWindow(0);
        goto tail;
    case 5:
        if (bTimerActive == 0)
            goto default_buttons;
        pParentView->btnDialogUp.SetWindowPos(pParentView, l, dtop, 0x10, 0x10, 4);
        pParentView->btnDialogDown.SetWindowPos(pParentView, rectUp.left, rectUp.top, 0x10, 0x10, 4);
        pParentView->btnDialogClose.SetWindowPos(pParentView, rectClose.left, rectClose.top, 0x10, 0x10, 4);
        pParentView->btnDialogClose.EnableWindow(0);
        pParentView->btnDialogClose.ShowWindow(5);
        pParentView->btnDialogDown.EnableWindow(0);
        pParentView->btnDialogDown.ShowWindow(5);
        pParentView->btnDialogUp.EnableWindow(1);
        pParentView->btnDialogUp.ShowWindow(5);
        break;
    case 2:
    case 3:
    case 4:
    default_buttons:
        pParentView->btnDialogClose.SetWindowPos(pParentView, l, t, 0x10, 0x10, 4);
        pParentView->btnDialogClose.EnableWindow(1);
        pParentView->btnDialogClose.ShowWindow(5);
        pParentView->btnDialogDown.EnableWindow(0);
        pParentView->btnDialogDown.ShowWindow(0);
        pParentView->btnDialogUp.EnableWindow(0);
        pParentView->btnDialogUp.ShowWindow(0);
        break;
    default:
        goto tail;
    }
tail:
    ReleaseDC(pParentView->m_hWnd, pDC->m_hDC);
    pParentView->DrawGameArea(0);
    ::SendMessage(pParentView->wndDialogText.m_hWnd, WM_SETREDRAW, 1, 0);
    pParentView->wndDialogText.ShowWindow(5);
    pParentView->btnDialogClose.ShowWindow(5);
}

// FUNCTION: YODA 0x00417c90
// TextDialog::ScrollTextLine — scroll the child edit down one line (unless already at the
// bottom), then re-enable/disable the up/down/close buttons for the new position.
// EFFECTIVE (align 12, 43/43 insns): one instruction shift — cl schedules the pParentView
// load ([esi+0xc0]) two bytes earlier than ours around the SendMessage arg pushes. A local
// cache made it far worse (align 120); the original re-reads the field. G1.
void TextDialog::ScrollTextLine()
{
    if (nScrollLine < nTotalLines)
    {
        ::SendMessage(pParentView->wndDialogText.m_hWnd, EM_LINESCROLL, 0, 1);
        nScrollLine++;
    }
    if (nTotalLines == nScrollLine)
    {
        pParentView->btnDialogUp.EnableWindow(0);
        pParentView->btnDialogClose.EnableWindow(1);
        pParentView->SetFocus();
        bAtBottom = 1;
    }
    else
    {
        pParentView->btnDialogUp.EnableWindow(1);
    }
    if (5 < nScrollLine)
        pParentView->btnDialogDown.EnableWindow(1);
}

// FUNCTION: YODA 0x00417d30
// TextDialog::ScrollTextLine2 — scroll the child edit up one line (unless at the top, line 5).
// EFFECTIVE (align 12, 37/37 insns): same one-instruction pParentView-load schedule shift as
// ScrollTextLine. Removing the `line` local (compare nScrollLine in memory directly, dec [mem])
// dropped the extra insns to a clean match-but-for-the-shift. G1.
void TextDialog::ScrollTextLine2()
{
    if (5 < nScrollLine)
    {
        ::SendMessage(pParentView->wndDialogText.m_hWnd, EM_LINESCROLL, 0, -1);
        nScrollLine--;
    }
    if (nScrollLine == 5)
    {
        pParentView->btnDialogDown.EnableWindow(0);
        pParentView->SetFocus();
    }
    else
    {
        pParentView->btnDialogDown.EnableWindow(1);
    }
    if (nScrollLine < nTotalLines)
        pParentView->btnDialogUp.EnableWindow(1);
}

// FUNCTION: YODA 0x00417dc0
// TextDialog::UpdateDialogButtons — on a scroll-button auto-repeat (BM_GETSTATE pushed),
// step the text one line in that direction.
// Takes an unused 4-byte stack arg (ret 4; callers push 1) — a __thiscall with a dead param,
// so declared `(int nUnused)`. EFFECTIVE (align 12, 47/47 insns): the same pParentView-load
// schedule shift as the scroll helpers. G1.
void TextDialog::UpdateDialogButtons(int nUnused)
{
    UINT st = ::SendMessage(pParentView->btnDialogUp.m_hWnd, BM_GETSTATE, 0, 0);
    if (pParentView->btnDialogUp.IsWindowEnabled() && (st & 4))
    {
        ScrollTextLine();
        return;
    }
    st = ::SendMessage(pParentView->btnDialogDown.m_hWnd, BM_GETSTATE, 0, 0);
    if (pParentView->btnDialogDown.IsWindowEnabled() && (st & 4))
        ScrollTextLine2();
}

// ===========================================================================
// Options-dialog cluster (0x417e50-0x4186e0). Three near-identical CDialog slider
// dialogs. NOTE: in .text these follow StatsDlg (0x416810) and the game TextDialog
// (0x416b90), which are transcribed separately — until those land above, these three
// blocks sit at a slightly displaced TU position (may read EFFECTIVE where they will
// be EXACT once the gap is filled).
// RESULTS: all 3 ctors + all 3 OnInitDialog EXACT; the 3 OnHScroll are the byte-identical
// clone family (lesson #7) - DifficultyDlg DIFF(5), GameSpeed/WorldSize larger (they get
// distinct reg allocations purely by TU position, like ParseZaux/Zax2/Zax3). Expect them
// to converge once StatsDlg/TextDialog fill the .text gap; parked for G1.
// ===========================================================================

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00417e50
// DifficultyDlg::DifficultyDlg — CDialog(template 0x6f).
// ---------------------------------------------------------------------------
DifficultyDlg::DifficultyDlg(CWnd *pParent) : CDialog(0x6f, pParent)
{
}

// FUNCTION: YODA 0x00417ec0  (??_GDifficultyDlg@@UAEPAXI@Z — compiler-emitted scalar dtor,
// 97B: EH-framed body + handler thunk @0x417f0f + dtor funclet @0x417f19 that tail-jmps
// ~CDialog. Our COMDAT is the same 97B; the funclet's jmp target is a masked reloc.)

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00417f30
// DifficultyDlg::DoDataExchange — EMPTY, does NOT call CDialog::DoDataExchange (the
// binary is a bare RET 4; vtable-slot-pinned at +0x88, same delta as StatsDlg's DDX).
// The dev deleted the ClassWizard base call (no DDX fields — OnHScroll reads the
// scrollbar directly). Same in GameSpeedDlg/WorldSizeDlg.
// ---------------------------------------------------------------------------
void DifficultyDlg::DoDataExchange(CDataExchange *pDX)
{
}

// FUNCTION: YODA 0x00417f40  (?GetMessageMap@DifficultyDlg@@MBEPBUAFX_MSGMAP@@XZ)
BEGIN_MESSAGE_MAP(DifficultyDlg, CDialog)
    ON_WM_HSCROLL()
END_MESSAGE_MAP()

// FUNCTION: YODA 0x00417f50
BOOL DifficultyDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    CenterWindow();
    CWnd *pCtrl = GetDlgItem(0x67);
    ::SetScrollRange(pCtrl->m_hWnd, SB_CTL, 1, 100, FALSE);
    ::SetScrollPos(pCtrl->m_hWnd, SB_CTL, m_nValue, TRUE);
    return TRUE;
}

// FUNCTION: YODA 0x00417fa0
void DifficultyDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
    CWnd *pCtrl = GetDlgItem(0x67);
    if (pCtrl == pScrollBar)
    {
        int nVal = m_nValue;
        int nMin = 1;
        int nMax = 100;
        ::GetScrollRange(pScrollBar->m_hWnd, SB_CTL, &nMin, &nMax);
        int nPage = ((nMax - nMin) + 1) / 10;
        switch (nSBCode)
        {
        case SB_LINEUP:
            nVal = ::GetScrollPos(pScrollBar->m_hWnd, SB_CTL);
            if (nMin < nVal)
                nVal--;
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nVal, TRUE);
            break;
        case SB_LINEDOWN:
            nVal = ::GetScrollPos(pScrollBar->m_hWnd, SB_CTL);
            if (nVal < nMax)
                nVal++;
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nVal, TRUE);
            break;
        case SB_PAGEUP:
            nVal = ::GetScrollPos(pScrollBar->m_hWnd, SB_CTL);
            nVal -= nPage;
            if (nVal <= nMin)
                nVal = nMin;
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nVal, TRUE);
            break;
        case SB_PAGEDOWN:
            nVal = ::GetScrollPos(pScrollBar->m_hWnd, SB_CTL);
            nVal += nPage;
            if (nMax <= nVal)
                nVal = nMax;
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nVal, TRUE);
            break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nPos, TRUE);
            nVal = nPos;
            break;
        case SB_TOP:
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nMin, TRUE);
            nVal = nMin;
            break;
        case SB_BOTTOM:
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nMax, TRUE);
            nVal = nMax;
            break;
        }
        m_nValue = nVal;
    }
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00418130
// GameSpeedDlg::GameSpeedDlg — CDialog(template 0xd7).
// ---------------------------------------------------------------------------
GameSpeedDlg::GameSpeedDlg(CWnd *pParent) : CDialog(0xd7, pParent)
{
}

// FUNCTION: YODA 0x004181a0  (??_GGameSpeedDlg@@UAEPAXI@Z — compiler-emitted scalar dtor,
// same 97B EH shape as DifficultyDlg's)

// FUNCTION: YODA 0x00418210
// GameSpeedDlg::DoDataExchange — EMPTY (bare RET 4; see DifficultyDlg::DoDataExchange).
void GameSpeedDlg::DoDataExchange(CDataExchange *pDX)
{
}

// FUNCTION: YODA 0x00418220  (?GetMessageMap@GameSpeedDlg@@MBEPBUAFX_MSGMAP@@XZ)
BEGIN_MESSAGE_MAP(GameSpeedDlg, CDialog)
    ON_WM_HSCROLL()
END_MESSAGE_MAP()

// FUNCTION: YODA 0x00418230
BOOL GameSpeedDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    CenterWindow();
    CWnd *pCtrl = GetDlgItem(0x8f);
    ::SetScrollRange(pCtrl->m_hWnd, SB_CTL, 1, 0x5a, FALSE);
    ::SetScrollPos(pCtrl->m_hWnd, SB_CTL, m_nValue, TRUE);
    return TRUE;
}

// FUNCTION: YODA 0x00418280
void GameSpeedDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
    CWnd *pCtrl = GetDlgItem(0x8f);
    if (pCtrl == pScrollBar)
    {
        int nVal = m_nValue;
        int nMin = 1;
        int nMax = 0x5a;
        ::GetScrollRange(pScrollBar->m_hWnd, SB_CTL, &nMin, &nMax);
        int nPage = ((nMax - nMin) + 1) / 10;
        switch (nSBCode)
        {
        case SB_LINEUP:
            nVal = ::GetScrollPos(pScrollBar->m_hWnd, SB_CTL);
            if (nMin < nVal)
                nVal--;
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nVal, TRUE);
            break;
        case SB_LINEDOWN:
            nVal = ::GetScrollPos(pScrollBar->m_hWnd, SB_CTL);
            if (nVal < nMax)
                nVal++;
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nVal, TRUE);
            break;
        case SB_PAGEUP:
            nVal = ::GetScrollPos(pScrollBar->m_hWnd, SB_CTL);
            nVal -= nPage;
            if (nVal <= nMin)
                nVal = nMin;
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nVal, TRUE);
            break;
        case SB_PAGEDOWN:
            nVal = ::GetScrollPos(pScrollBar->m_hWnd, SB_CTL);
            nVal += nPage;
            if (nMax <= nVal)
                nVal = nMax;
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nVal, TRUE);
            break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nPos, TRUE);
            nVal = nPos;
            break;
        case SB_TOP:
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nMin, TRUE);
            nVal = nMin;
            break;
        case SB_BOTTOM:
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nMax, TRUE);
            nVal = nMax;
            break;
        }
        m_nValue = nVal;
    }
}

// ---------------------------------------------------------------------------
// FUNCTION: YODA 0x00418410
// WorldSizeDlg::WorldSizeDlg — CDialog(template 0xda).
// ---------------------------------------------------------------------------
WorldSizeDlg::WorldSizeDlg(CWnd *pParent) : CDialog(0xda, pParent)
{
}

// FUNCTION: YODA 0x00418480  (??_GWorldSizeDlg@@UAEPAXI@Z — compiler-emitted scalar dtor,
// same 97B EH shape as DifficultyDlg's)

// FUNCTION: YODA 0x004184f0
// WorldSizeDlg::DoDataExchange — EMPTY (bare RET 4; see DifficultyDlg::DoDataExchange).
void WorldSizeDlg::DoDataExchange(CDataExchange *pDX)
{
}

// FUNCTION: YODA 0x00418500  (?GetMessageMap@WorldSizeDlg@@MBEPBUAFX_MSGMAP@@XZ)
BEGIN_MESSAGE_MAP(WorldSizeDlg, CDialog)
    ON_WM_HSCROLL()
END_MESSAGE_MAP()

// FUNCTION: YODA 0x00418510
BOOL WorldSizeDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    CenterWindow();
    CWnd *pCtrl = GetDlgItem(0x90);
    ::SetScrollRange(pCtrl->m_hWnd, SB_CTL, 1, 3, TRUE);
    ::SetScrollPos(pCtrl->m_hWnd, SB_CTL, m_nValue, TRUE);
    return TRUE;
}

// FUNCTION: YODA 0x00418560
void WorldSizeDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
    CWnd *pCtrl = GetDlgItem(0x90);
    if (pCtrl == pScrollBar)
    {
        int nVal = m_nValue;
        int nMin = 1;
        int nMax = 3;
        ::GetScrollRange(pScrollBar->m_hWnd, SB_CTL, &nMin, &nMax);
        int nPage = ((nMax - nMin) + 1) / 10;
        switch (nSBCode)
        {
        case SB_LINEUP:
            nVal = ::GetScrollPos(pScrollBar->m_hWnd, SB_CTL);
            if (nMin < nVal)
                nVal--;
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nVal, TRUE);
            break;
        case SB_LINEDOWN:
            nVal = ::GetScrollPos(pScrollBar->m_hWnd, SB_CTL);
            if (nVal < nMax)
                nVal++;
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nVal, TRUE);
            break;
        case SB_PAGEUP:
            nVal = ::GetScrollPos(pScrollBar->m_hWnd, SB_CTL);
            nVal -= nPage;
            if (nVal <= nMin)
                nVal = nMin;
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nVal, TRUE);
            break;
        case SB_PAGEDOWN:
            nVal = ::GetScrollPos(pScrollBar->m_hWnd, SB_CTL);
            nVal += nPage;
            if (nMax <= nVal)
                nVal = nMax;
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nVal, TRUE);
            break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nPos, TRUE);
            nVal = nPos;
            break;
        case SB_TOP:
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nMin, TRUE);
            nVal = nMin;
            break;
        case SB_BOTTOM:
            ::SetScrollPos(pScrollBar->m_hWnd, SB_CTL, nMax, TRUE);
            nVal = nMax;
            break;
        }
        m_nValue = nVal;
    }
}

// FUNCTION: YODA 0x004186e0
// TriPoint::TriPoint — the game's speech-bubble point type's empty default ctor. Emitted
// out-of-line (last function in the TU) as `mov eax,ecx; ret` (returns this, does nothing);
// TextDialog::Layout's `TriPoint point[3];` array construction calls it 3x.
TriPoint::TriPoint()
{
}

// FUNCTION: YODA 0x00408690
// FUNCTION: YODA 0x004086b0
// InvScrollBar::~InvScrollBar — EXPLICIT dtor calling DestroyWindow(). Declaring the dtor (vs.
// leaving it implicit) forces MSVC to SPLIT destruction into a separate ??1 (0x4086b0, the
// 91-byte SEH-framed body: vtable reset to 0x44b578, SEH-protected DestroyWindow(this) [unwinds
// to ~CScrollBar if it throws], then the base ~CScrollBar => ~CWnd) and a THIN ??_G (0x408690,
// 30-byte scalar-deleting: call ??1 then operator delete if flag&1). Two proofs the body is
// DestroyWindow() (not empty): (a) an empty body emits only 79 bytes; (b) the disasm at 0x4086b0
// shows CALL CWnd::DestroyWindow inside the try. An implicit dtor would instead inline the whole
// body into ??_G with no ??1 (MFC-matching lesson #1). Both COMDATs byte-match (match.py; note
// verify.py mis-pairs this clone family). DEFINED HERE (end of TU) so the added lines do not
// shift #line provenance of the functions above and rotate their dial — placement is
// codegen-neutral for THIS function (an SEH thunk dtor whose bytes don't depend on TU phase).
InvScrollBar::~InvScrollBar()
{
    DestroyWindow();
}
