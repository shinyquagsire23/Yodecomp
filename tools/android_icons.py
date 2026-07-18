#!/usr/bin/env python3
# android_icons.py <game_exe> <res_dir> [--indy <DESKADV.EXE>]
#
# Generate Android launcher icons (res/mipmap-<density>/ic_launcher.png) from the game's own
# 32x32 application icon — the SAME source + crisp nearest-neighbour upscaler the macOS .icns
# build uses (tools/make_icns.py). Pure stdlib PNG writer (no PIL), so the `apk` target has no
# extra dependency. Never fails the build for a cosmetic reason: on any error it writes a solid
# fallback tile so gradle always finds @mipmap/ic_launcher.
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_icns

# Android density buckets -> launcher icon px (mdpi/hdpi/xhdpi/xxhdpi/xxxhdpi).
DENSITIES = [("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)]


def solid_rows(size, rgba):
    return [bytearray(rgba * size) for _ in range(size)]


def main():
    exe, res_dir = sys.argv[1], sys.argv[2]
    indy_exe = None
    a = sys.argv[3:]
    while a:
        if a[0] == '--indy':
            indy_exe = a[1]; a = a[2:]
        else:
            raise SystemExit(f"unknown arg {a[0]}")

    try:
        w, h, rows = make_icns.decode_dib_icon(make_icns.game_icon_dib(exe, indy_exe))
    except Exception as e:                                   # cosmetic only — never break the build
        sys.stderr.write(f"android_icons: falling back to a solid tile ({e})\n")
        w = h = 32
        rows = solid_rows(32, bytes((60, 90, 160, 255)))     # muted blue

    for bucket, size in DENSITIES:
        d = os.path.join(res_dir, f"mipmap-{bucket}")
        os.makedirs(d, exist_ok=True)
        scaled = make_icns.scale_nn(rows, w, h, size, size)
        make_icns.write_png(os.path.join(d, "ic_launcher.png"), size, size, scaled)
        make_icns.write_png(os.path.join(d, "ic_launcher_round.png"), size, size, scaled)
    print(f"android_icons: wrote ic_launcher from {w}x{h} icon into {res_dir}/mipmap-*")


if __name__ == "__main__":
    main()
