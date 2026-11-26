#!/usr/bin/env python3
"""
VLW (Processing / TFT_eSPI / LovyanGFX "smooth font") generátor magyar ékezetekkel.

Használat:
    python3 tools/make_vlw.py
Kimenet: src/fonts_hu.h  (PROGMEM tömbök: font_hu13, font_hu16, font_hu22, font_hu40)

A font: DejaVu Sans Bold (szabad licenc, Bitstream Vera / DejaVu).
"""
import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
SIZES = [13, 16, 22, 40]

CHARS = [chr(c) for c in range(32, 127)] + list("ÁÉÍÓÖŐÚÜŰáéíóöőúüű°←→")


def make_vlw(size: int) -> bytes:
    font = ImageFont.truetype(FONT_PATH, size)
    ascent, descent = font.getmetrics()
    glyphs = []
    for ch in sorted(set(CHARS), key=ord):
        adv = int(round(font.getlength(ch)))
        if ch == " ":
            glyphs.append((ord(ch), 0, 0, adv, 0, 0, b""))
            continue
        x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")
        w, h = x1 - x0, y1 - y0
        if w <= 0 or h <= 0:
            glyphs.append((ord(ch), 0, 0, adv, 0, 0, b""))
            continue
        img = Image.new("L", (w, h), 0)
        ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255, anchor="ls")
        glyphs.append((ord(ch), h, w, adv, -y0, x0, img.tobytes()))

    out = bytearray()
    # fejléc: glyph-szám, verzió, méret, (nem használt), ascent, descent
    out += struct.pack(">6i", len(glyphs), 11, size, 0, ascent, descent)
    for uni, h, w, adv, dy, dx, _ in glyphs:
        out += struct.pack(">7i", uni, h, w, adv, dy, dx, 0)
    for *_, bmp in glyphs:
        out += bmp
    return bytes(out)


def to_c_array(name: str, data: bytes) -> str:
    lines = [f"static const uint8_t {name}[] PROGMEM = {{"]
    for i in range(0, len(data), 20):
        lines.append("  " + ", ".join(f"0x{b:02X}" for b in data[i:i + 20]) + ",")
    lines.append("};")
    return "\n".join(lines)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    target = os.path.join(here, "..", "src", "fonts_hu.h")
    parts = [
        "// GENERÁLT FÁJL - ne szerkeszd kézzel! Forrás: tools/make_vlw.py",
        "// Font: DejaVu Sans Bold (DejaVu fonts license). VLW formátum, magyar ékezetekkel.",
        "#pragma once",
        "#include <Arduino.h>",
        "",
    ]
    for s in SIZES:
        data = make_vlw(s)
        parts.append(f"// {s} px, {len(data)} byte")
        parts.append(to_c_array(f"font_hu{s}", data))
        parts.append("")
    with open(target, "w", encoding="utf-8") as f:
        f.write("\n".join(parts))
    print("OK ->", os.path.normpath(target))


if __name__ == "__main__":
    sys.exit(main())
