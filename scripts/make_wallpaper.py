#!/usr/bin/env python3
"""Convert an ordinary image (PNG/JPG/…) into a sleep-screen wallpaper BMP for the
Xteink X4 Pro.

The X4 Pro's e-ink panel is 800x480 with four native gray levels: 0, 85, 170, 255.
The firmware's BMP reader (lib/GfxRenderer/Bitmap.cpp) has a "native palette" fast
path: if every palette entry already sits on one of those four levels, it skips
dithering entirely and shows the image exactly as written. So this tool does the
dithering on the desktop and hands the device a BMP it renders pixel-for-pixel.

  gray4 (default): Floyd-Steinberg to {0,85,170,255}, 8-bit paletted BMP.
  bw            : Floyd-Steinberg to {0,255}, 1-bit BMP.

The sleep screen scales/centres whatever it loads, but pre-fitting to the exact
panel size means no on-device rescale, so the dither stays crisp.

Usage:
  scripts/make_wallpaper.py photo.jpg                       -> photo.bmp (gray4, cover)
  scripts/make_wallpaper.py photo.jpg -o sleep.bmp --mode bw
  scripts/make_wallpaper.py *.jpg --outdir out/ --fit contain

Then drop the .bmp on the SD card and, in the file browser, pick "Set as sleep
screen" (copies to /sleep.bmp) or "Add to rotation" (copies into /.sleep/).

Requires Pillow:  pip install pillow
"""

import argparse
import os
import sys

try:
    from PIL import Image, ImageOps
except ImportError:
    sys.exit("This tool needs Pillow. Install it with:  pip install pillow")

# The panel's four native gray levels. Values where (lum >> 6) is lossless, matching
# Bitmap.cpp's nativePalette check.
GRAY4_LEVELS = (0, 85, 170, 255)
BW_LEVELS = (0, 255)

PANEL_W, PANEL_H = 800, 480


def parse_size(text):
    try:
        w, h = text.lower().split("x")
        return int(w), int(h)
    except ValueError:
        raise argparse.ArgumentTypeError(f"--size wants WxH (e.g. 800x480), got {text!r}")


def fit_image(img, size, mode, background):
    """Resize to `size`. cover = fill and crop; contain = fit and pad with `background`."""
    if mode == "cover":
        return ImageOps.fit(img, size, method=Image.LANCZOS, centering=(0.5, 0.5))
    # contain
    fitted = img.copy()
    fitted.thumbnail(size, Image.LANCZOS)
    canvas = Image.new("L", size, background)
    canvas.paste(fitted, ((size[0] - fitted.width) // 2, (size[1] - fitted.height) // 2))
    return canvas


def floyd_steinberg(pixels, w, h, levels):
    """In-place Floyd-Steinberg error diffusion onto an arbitrary set of gray levels.

    `pixels` is a mutable list of ints (row-major). Returns a new list of the same
    length holding the chosen level VALUES (not indices).
    """
    # Work in floats for the diffused error.
    buf = [float(p) for p in pixels]
    out = [0] * (w * h)

    def nearest(v):
        best = levels[0]
        bestd = abs(v - best)
        for lv in levels[1:]:
            d = abs(v - lv)
            if d < bestd:
                best, bestd = lv, d
        return best

    for y in range(h):
        row = y * w
        # Serpentine scan reduces directional worm artefacts.
        rng = range(w) if (y % 2 == 0) else range(w - 1, -1, -1)
        fwd = y % 2 == 0
        for x in rng:
            i = row + x
            old = buf[i]
            new = nearest(old)
            out[i] = new
            err = old - new
            if fwd:
                if x + 1 < w:
                    buf[i + 1] += err * 7 / 16
                if y + 1 < h:
                    if x > 0:
                        buf[i + w - 1] += err * 3 / 16
                    buf[i + w] += err * 5 / 16
                    if x + 1 < w:
                        buf[i + w + 1] += err * 1 / 16
            else:  # scanning right-to-left; mirror the kernel
                if x - 1 >= 0:
                    buf[i - 1] += err * 7 / 16
                if y + 1 < h:
                    if x + 1 < w:
                        buf[i + w + 1] += err * 3 / 16
                    buf[i + w] += err * 5 / 16
                    if x - 1 >= 0:
                        buf[i + w - 1] += err * 1 / 16
    return out


def convert(in_path, out_path, size, mode, fit, background):
    img = Image.open(in_path)
    img = ImageOps.exif_transpose(img)  # honour phone orientation tags
    img = img.convert("L")
    img = fit_image(img, size, fit, background)
    w, h = img.size

    if mode == "bw":
        # PIL's own Floyd-Steinberg for the 1-bit case; saves as a true 1bpp BMP.
        bw = img.convert("1")  # dither=FLOYDSTEINBERG by default
        bw.save(out_path, format="BMP")
        return w, h, "1-bit B/W"

    # gray4: dither to the four native levels ourselves, then emit an 8-bit paletted
    # BMP whose palette is exactly {0,85,170,255} -> firmware's native-palette path.
    values = floyd_steinberg(list(img.tobytes()), w, h, GRAY4_LEVELS)
    level_index = {v: i for i, v in enumerate(GRAY4_LEVELS)}
    indexed = Image.new("P", (w, h))
    indexed.putdata([level_index[v] for v in values])
    palette = []
    for v in GRAY4_LEVELS:
        palette += [v, v, v]
    palette += [0, 0, 0] * (256 - len(GRAY4_LEVELS))  # pad unused entries (stay native)
    indexed.putpalette(palette)
    indexed.save(out_path, format="BMP")
    return w, h, "4-level grayscale (native palette)"


def main():
    ap = argparse.ArgumentParser(
        description="Convert images into X4 Pro sleep-screen wallpaper BMPs.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    ap.add_argument("images", nargs="+", help="input image(s)")
    ap.add_argument("-o", "--output", help="output path (single input only)")
    ap.add_argument("--outdir", help="write outputs here, named <input>.bmp")
    ap.add_argument("--mode", choices=("gray4", "bw"), default="gray4",
                    help="gray4 = 4-level grayscale (default); bw = 1-bit black/white")
    ap.add_argument("--size", type=parse_size, default=(PANEL_W, PANEL_H),
                    help="target WxH (default 800x480, the panel size)")
    ap.add_argument("--fit", choices=("cover", "contain"), default="cover",
                    help="cover = fill & crop (default); contain = fit & letterbox")
    ap.add_argument("--background", type=int, default=255,
                    help="letterbox fill for --fit contain, 0-255 (default 255=white)")
    args = ap.parse_args()

    if args.output and len(args.images) > 1:
        ap.error("-o/--output takes a single input; use --outdir for batches")

    rc = 0
    for in_path in args.images:
        if args.output:
            out_path = args.output
        else:
            base = os.path.splitext(os.path.basename(in_path))[0] + ".bmp"
            out_path = os.path.join(args.outdir, base) if args.outdir else \
                os.path.join(os.path.dirname(in_path), base)
        if args.outdir:
            os.makedirs(args.outdir, exist_ok=True)
        try:
            w, h, desc = convert(in_path, out_path, args.size, args.mode, args.fit,
                                 args.background)
            kb = os.path.getsize(out_path) / 1024
            print(f"{in_path}  ->  {out_path}   {w}x{h}  {desc}  ({kb:.0f} KB)")
        except Exception as e:  # noqa: BLE001 - report per-file and keep going
            print(f"{in_path}  ->  FAILED: {e}", file=sys.stderr)
            rc = 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
