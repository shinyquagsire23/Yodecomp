#!/usr/bin/env python3
# make_res.py — build the .res for the EXTENDED configs (YODA_FULL / GAME_INDY).
#
# The byte-match anchor build uses tools/extract_res.py (verbatim YodaDemo .rsrc). The extended
# configs keep Yoda's .rsrc as the base — our decompiled code references YodaDemo's integer
# resource IDs (dialogs/menus/bitmaps/controls), so a wholesale swap would break the UI — and
# override only the resources that must reflect the game/variant:
#
#   --full <Yodesk.exe>    (YODA_FULL): the retail Yoda About dialog (id 100), so the About box
#                          reads "Yoda(tm) Stories" instead of the demo's "Yoda(tm) Stories Demo".
#   --indy <DESKADV.EXE>   (GAME_INDY): the app icon + title string (see the icon/title code), the
#                          About dialog rewritten with Indy's title + credits ("Indiana Jones
#                          and his Desktop Adventures", "The Desktop Adventures Team", …), AND
#                          Indy's real RT_MENU id 2 (no Statistics item — retail Indy has neither
#                          the menu item nor dialog 0xe1; every command id in Indy's menu already
#                          exists in the shared dispatch space, verified 2026-07-11).
#
# Usage: python3 tools/make_res.py <YodaDemo.exe> <out.res> [--full <Yodesk.exe>] [--indy <DESKADV.exe>]
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from reslib import (pe_leaves, pe_leaf, ne_resources, patch_stringtable, parse_dlg32,
                    build_dlg32, emit_res, RT_ICON, RT_GROUP_ICON, RT_STRING, RT_DIALOG)

IDR_MAINFRAME = 2
IDD_ABOUTBOX = 100
AFX_IDS_APP_TITLE = 0xE000       # 57344
INDY_ICON_ID = 901               # a free RT_ICON ordinal (Yoda uses 1..11)

# Indy About text (verbatim from DESKADV.EXE's own DIALOG 100 / string table).
INDY_TITLE = "Indiana Jones and his Desktop Adventures"
INDY_APP_TITLE = "Desktop Adventures"        # doc-template title + AFX_IDS_APP_TITLE (real DESKADV title)
INDY_COPYRIGHT = "© 1996 LucasArts Entertainment Company"
INDY_CREDITS = ("              The Desktop Adventures Team\n\n"
                "        Hal Barwood    Wayne Cline\n"
                "        Mark Crowley   Reed Derleth\n"
                "        Paul LeFevre    Tom Payne")
INDY_ABOUT_CAPTION = "About Desktop Adventures"


def indy_about(demo_dialog):
    """Rewrite YodaDemo's 32-bit About (id 100) with Indy's title/copyright/credits."""
    hdr, ctrls = parse_dlg32(demo_dialog)
    hdr['caption'] = INDY_ABOUT_CAPTION
    x, y, cx, cy = hdr['rect']
    for c in ctrls:
        k, v = c['text']
        if k == 'ord' and v == 101:                 # the About icon -> the (Indy) app icon
            c['text'] = ('ord', IDR_MAINFRAME)
        elif k == 'sz' and v.startswith("Yoda(tm) Stories"):
            c['text'] = ('sz', INDY_TITLE)
            c['rect'] = (2, c['rect'][1], cx - 4, c['rect'][3])   # widen for the longer title
        elif k == 'sz' and ('Lucasfilm' in v or v.startswith("©")):
            c['text'] = ('sz', INDY_COPYRIGHT)
        elif k == 'sz' and 'Team Yoda' in v:
            c['text'] = ('sz', INDY_CREDITS)
    return build_dlg32(hdr, ctrls)


RT_MENU = 4

def ne_menu_to_win32(data):
    """Convert a 16-bit NE MENU template (ANSI strings) to the Win32 MENUITEMTEMPLATE shape
    (identical WORD flags/id layout, UTF-16 strings) that LoadMenu/mfxmenu.cpp consume."""
    import struct
    out = bytearray(data[:4])                      # MENUITEMTEMPLATEHEADER (ver 0, offset 0)
    off = 4
    def cvt():
        nonlocal off
        while off < len(data):
            flags = struct.unpack_from('<H', data, off)[0]; off += 2
            out.extend(struct.pack('<H', flags))
            if not flags & 0x10:                   # not MF_POPUP: WORD command id
                out.extend(data[off:off + 2]); off += 2
            end = data.index(b'\0', off)
            out.extend(data[off:end].decode('cp1252').encode('utf-16-le') + b'\0\0')
            off = end + 1
            if flags & 0x10:
                cvt()                              # popup: nested item list follows
            if flags & 0x80:                       # MF_END closes this level
                return
    cvt()
    return bytes(out)


def indy_icon(indy_exe):
    """Return (group_icon_dir_bytes with nID->INDY_ICON_ID, icon_image_bytes) from DESKADV.EXE."""
    import struct
    res = ne_resources(indy_exe)
    grp = bytearray(res[(RT_GROUP_ICON, IDR_MAINFRAME)])
    member = struct.unpack_from('<H', grp, 6 + 12)[0]
    icon = res[(RT_ICON, member)]
    struct.pack_into('<H', grp, 6 + 12, INDY_ICON_ID)
    return bytes(grp), icon


def main():
    yoda_exe, out = sys.argv[1], sys.argv[2]
    full_exe = indy_exe = None
    a = sys.argv[3:]
    while a:
        if a[0] == '--full':
            full_exe = a[1]; a = a[2:]
        elif a[0] == '--indy':
            indy_exe = a[1]; a = a[2:]
        else:
            raise SystemExit(f"unknown arg {a[0]}")

    leaves = pe_leaves(yoda_exe)
    notes = []

    if full_exe:
        retail_about = pe_leaf(full_exe, RT_DIALOG, IDD_ABOUTBOX)
        assert retail_about is not None, "retail DIALOG 100 not found"
        for lv in leaves:
            if lv[0] == ('I', RT_DIALOG) and lv[1] == ('I', IDD_ABOUTBOX):
                lv[3] = retail_about
        notes.append("full About (no 'Demo')")

    if indy_exe:
        grp, icon = indy_icon(indy_exe)
        kept = []
        for lv in leaves:
            (t, n, l, d) = lv
            if t == ('I', RT_GROUP_ICON) and n == ('I', IDR_MAINFRAME):
                continue                                    # replaced below
            if t == ('I', RT_ICON) and n == ('I', 11):
                continue                                    # Yoda's app icon image, now unused
            if t == ('I', RT_STRING) and n == ('I', 1):
                lv = [t, n, l, patch_stringtable(d, IDR_MAINFRAME, INDY_APP_TITLE)]
            elif t == ('I', RT_STRING) and n == ('I', AFX_IDS_APP_TITLE // 16 + 1):
                lv = [t, n, l, patch_stringtable(d, AFX_IDS_APP_TITLE % 16, INDY_APP_TITLE)]
            elif t == ('I', RT_DIALOG) and n == ('I', IDD_ABOUTBOX):
                lv = [t, n, l, indy_about(d)]
            elif t == ('I', RT_MENU) and n == ('I', IDR_MAINFRAME):
                lv = [t, n, l, ne_menu_to_win32(ne_resources(indy_exe)[(RT_MENU, IDR_MAINFRAME)])]
            kept.append(lv)
        lang = ('I', 1033)
        kept.append([('I', RT_GROUP_ICON), ('I', IDR_MAINFRAME), lang, grp])
        kept.append([('I', RT_ICON), ('I', INDY_ICON_ID), lang, icon])
        leaves = kept
        notes.append(f"Indy icon(#{INDY_ICON_ID}) + title({INDY_APP_TITLE!r}) + About + menu")

    emit_res(leaves, out)
    print(f"wrote {out}: {len(leaves)} leaves; " + ", ".join(notes) if notes else
          f"wrote {out}: {len(leaves)} leaves (base only)")


if __name__ == '__main__':
    main()
