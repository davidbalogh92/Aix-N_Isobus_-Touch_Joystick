#!/usr/bin/env python3
"""icon_preview.cpp kimenetéből PNG előnézet (a firmware fontjával, DejaVu Sans Bold)."""
import sys
from PIL import Image, ImageDraw, ImageFont

FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"


def c565(v):
    r = (v >> 11) & 0x1F
    g = (v >> 5) & 0x3F
    b = v & 0x1F
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)


def main(src, dst):
    lines = open(src).read().splitlines()
    imgs = []
    i = 0
    while i < len(lines):
        if lines[i].startswith("IMG"):
            _, w, h = lines[i].split()
            w, h = int(w), int(h)
            data = lines[i + 1]
            im = Image.new("RGB", (w, h))
            pix = im.load()
            for k in range(w * h):
                pix[k % w, k // w] = c565(int(data[k * 4:k * 4 + 4], 16))
            d = ImageDraw.Draw(im)
            i += 2
            while lines[i] != "END":
                p = lines[i].split(" ", 6)
                cx, cy, sz, fg = int(p[1]), int(p[2]), int(p[3]), int(p[4], 16)
                f = ImageFont.truetype(FONT, sz)
                d.text((cx, cy), p[6], font=f, fill=c565(fg), anchor="mm")
                i += 1
            imgs.append(im)
        i += 1
    gap = 16
    W = sum(im.width for im in imgs) + gap * (len(imgs) + 1)
    H = max(im.height for im in imgs) + 2 * gap
    out = Image.new("RGB", (W, H), (255, 255, 255))
    x = gap
    for im in imgs:
        out.paste(im, (x, gap))
        x += im.width + gap
    out.save(dst)
    print("OK ->", dst)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
