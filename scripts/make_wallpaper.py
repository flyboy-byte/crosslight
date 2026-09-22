#!/usr/bin/env python3
"""Prep any image as an X4 Pro sleep-screen wallpaper.

    python3 scripts/make_wallpaper.py photo.jpg                 # -> photo_sleep.bmp
    python3 scripts/make_wallpaper.py photo.jpg -o sleep.bmp --preview check.png
    python3 scripts/make_wallpaper.py art.png --fit contain --contrast 1.2

Copy the result to the SD card as /sleep.bmp (single wallpaper) or into /.sleep/
(random rotation), then set Settings > Display > Sleep Screen to Custom.

Default output is 1-bit, dithered here: this unit's UC8279 panel draws sleep images
in black and white, and the firmware only dithers high-color images — a 4-gray image
gets thresholded instead, which posterizes it. Use --mode gray4 for SSD1677 units.
"""

import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageEnhance, ImageOps

WIDTH, HEIGHT = 480, 800
GRAY4 = np.array([0, 85, 170, 255], dtype=np.float32)


def fit_image(img, mode, background):
    if mode == "cover":
        return ImageOps.fit(img, (WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    img = ImageOps.contain(img, (WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    canvas = Image.new("L", (WIDTH, HEIGHT), background)
    canvas.paste(img, ((WIDTH - img.width) // 2, (HEIGHT - img.height) // 2))
    return canvas


def atkinson(gray, count):
    """Atkinson error diffusion (what the firmware uses) onto `count` evenly spaced levels.

    Returns level indices. Plain lists: numpy scalar ops in a per-pixel loop are far slower.
    """
    h, w = gray.shape
    step = 255.0 / (count - 1)
    rows = [list(map(float, r)) for r in gray]
    out = np.empty((h, w), dtype=np.uint8)
    for y in range(h):
        r0 = rows[y]
        r1 = rows[y + 1] if y + 1 < h else None
        r2 = rows[y + 2] if y + 2 < h else None
        orow = out[y]
        for x in range(w):
            old = r0[x]
            idx = int(old / step + 0.5)
            idx = 0 if idx < 0 else (count - 1 if idx >= count else idx)
            orow[x] = idx
            err = (old - idx * step) / 8.0
            if err:
                if x + 1 < w:
                    r0[x + 1] += err
                if x + 2 < w:
                    r0[x + 2] += err
                if r1 is not None:
                    if x > 0:
                        r1[x - 1] += err
                    r1[x] += err
                    if x + 1 < w:
                        r1[x + 1] += err
                if r2 is not None:
                    r2[x] += err
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", type=Path)
    ap.add_argument("-o", "--output", type=Path, help="output .bmp (default: <input>_sleep.bmp)")
    ap.add_argument("--mode", choices=["1bit", "gray4"], default="1bit")
    ap.add_argument("--fit", choices=["cover", "contain"], default="cover",
                    help="cover = fill the screen and crop; contain = whole image with borders")
    ap.add_argument("--background", choices=["white", "black"], default="white", help="border color for contain")
    ap.add_argument("--rotate", type=int, choices=[0, 90, 180, 270], default=0,
                    help="rotate counter-clockwise first, e.g. 90 to fill the screen with a landscape image")
    ap.add_argument("--brightness", type=float, default=1.0)
    ap.add_argument("--contrast", type=float, default=1.0)
    ap.add_argument("--gamma", type=float, default=1.0, help=">1 darkens midtones, <1 lightens")
    ap.add_argument("--preview", type=Path, help="also write a PNG of exactly what the device will show")
    args = ap.parse_args()

    img = ImageOps.exif_transpose(Image.open(args.input)).convert("L")
    if args.rotate:
        img = img.rotate(args.rotate, expand=True)
    img = fit_image(img, args.fit, 255 if args.background == "white" else 0)
    if args.brightness != 1.0:
        img = ImageEnhance.Brightness(img).enhance(args.brightness)
    if args.contrast != 1.0:
        img = ImageEnhance.Contrast(img).enhance(args.contrast)

    gray = np.asarray(img, dtype=np.float32)
    if args.gamma != 1.0:
        gray = 255.0 * (gray / 255.0) ** args.gamma

    levels = np.array([0, 255], dtype=np.float32) if args.mode == "1bit" else GRAY4
    idx = atkinson(gray, len(levels))
    shown = levels[idx].astype(np.uint8)

    out = args.output or args.input.with_name(args.input.stem + "_sleep.bmp")
    if args.mode == "1bit":
        Image.fromarray(shown).convert("1", dither=Image.Dither.NONE).save(out, "BMP")
    else:
        pal = Image.fromarray(idx, "P")
        pal.putpalette([v for g in GRAY4.astype(int) for v in (g, g, g)])
        pal.save(out, "BMP")
    if args.preview:
        Image.fromarray(shown).save(args.preview)
    print(f"wrote {out} ({WIDTH}x{HEIGHT}, {args.mode}, {out.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    sys.exit(main())
