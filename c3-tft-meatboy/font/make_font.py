#!/usr/bin/env python3
"""Genere font_chunky.h (polices Adafruit GFX, ASCII 32-126) a partir de la planche
specimen.png (alphabet manuscrit epais). Les glyphes sont extraits de l'image puis
reduits a plusieurs hauteurs (majuscule = 9, 15, 22, 30, 44 px) et seuilles en 1 bit.
Usage : python make_font.py  (ecrit ../font_chunky.h et preview.png)
Caracteres absents de la planche (< > [ ] } ~ ^ `) : < et > sont dessines a la main, le reste est vide.
"""
import sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

HERE = Path(__file__).parent
SRC = HERE / "specimen.png"
SIZES = [9, 15, 22, 30, 44]                  # hauteur de majuscule (px) de chaque police
CAP_SRC = 37.0                               # hauteur de majuscule dans l'image source
BASE_OFFSET = 91                             # cellule : haut de cellule -> ligne de base
CELL_TOPS = [-44, 68, 201, 312, 445, 578, 689]   # haut de chaque rangee de cellules (lu sur les bordures)
ROWS = [  # caracteres ASCII de chaque rangee, dans l'ordre ; None = glyphe non ASCII (ignore)
    list("ABCDEFGHIJKLMNO"),
    list("PQRSTUVWXYZ"),
    list("abcdefghijklmno"),
    list("pqrstuvwxyz"),
    list("0123456789"),
    ["$", None, None, None, None, None, "+", "-", "*", "/", None, "=", "%", None, '"'],
    ["'", "#", "@", "&", "_", "(", ")", ",", ".", ";", ":", None, "?", None, "!", "\\", "|", "{"],
]


def groups_for_row(dark, base):
    y0, y1 = max(0, base - 60), min(dark.shape[0], base + 35)
    cols = dark[y0:y1].any(axis=0)
    g, x = [], 0
    while x < len(cols):
        if cols[x]:
            s = x
            while x < len(cols) and cols[x]:
                x += 1
            g.append([s, x])
        else:
            x += 1
    merged = []                               # recolle les morceaux separes de 1-2 px (", ‰, %)
    for s, e in g:
        if merged and s - merged[-1][1] <= 2:
            merged[-1][1] = e
        else:
            merged.append([s, e])
    return merged


def extract():
    im = Image.open(SRC).convert("L")
    a = np.array(im)
    cov = 1.0 - a / 255.0                      # couverture (0 = blanc, 1 = noir)
    cov[a > 170] = 0.0                         # ignore les etiquettes grises de la planche
    dark = a < 90
    glyphs = {}
    for ri, chars in enumerate(ROWS):
        base = CELL_TOPS[ri] + BASE_OFFSET
        grp = groups_for_row(dark, base)
        if len(grp) != len(chars):
            print(f"rangee {ri}: {len(grp)} groupes pour {len(chars)} caracteres", file=sys.stderr)
            sys.exit(1)
        for (s, e), ch in zip(grp, chars):
            if ch is None:
                continue
            crop = cov[max(0, base - 70):base + 40, max(0, s - 4):e + 4]
            top = base - max(0, base - 70)
            glyphs[ch] = (crop, top, s - max(0, s - 4), e - s)     # image, ligne de base dans le crop, marge gauche, largeur d'encre
    return glyphs


def chevron(right):
    img = Image.new("L", (40, 110), 0)
    d = ImageDraw.Draw(img)
    pts = [(8, 30), (30, 52), (8, 74)] if right else [(32, 30), (10, 52), (32, 74)]
    d.line(pts, fill=255, width=9, joint="curve")
    arr = np.array(img) / 255.0
    return arr, 70, 8, 24


def render(glyph, H):
    crop, base, left, w = glyph
    k = H / CAP_SRC
    img = Image.fromarray((crop * 255).astype(np.uint8))
    nw, nh = max(1, round(img.width * k)), max(1, round(img.height * k))
    small = np.array(img.resize((nw, nh), Image.LANCZOS)) / 255.0
    bits = small > 0.42
    ys, xs = np.where(bits)
    if len(xs) == 0:
        return None
    base_s = round(base * k)
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    lead = round(left * k)
    return bits[y0:y1, x0:x1], x0 - lead + 0, y0 - base_s, round(w * k)


def build(glyphs, H):
    k = H / CAP_SRC
    bitmap, table = bytearray(), []
    for code in range(32, 127):
        ch = chr(code)
        if ch == " ":
            table.append((len(bitmap), 0, 0, round(16 * k) + 1, 0, 0)); continue
        g = glyphs.get(ch) or (chevron(True) if ch == ">" else chevron(False) if ch == "<" else None)
        r = render(g, H) if g else None
        if r is None:
            table.append((len(bitmap), 0, 0, round(16 * k) + 1, 0, 0)); continue
        bits, xo, yo, wi = r
        h, w = bits.shape
        bo = len(bitmap)
        acc, n = 0, 0
        for v in bits.flatten():
            acc = (acc << 1) | int(v); n += 1
            if n == 8:
                bitmap.append(acc); acc, n = 0, 0
        if n:
            bitmap.append(acc << (8 - n))
        adv = max(1, wi + round(6 * k) + 1)
        table.append((bo, w, h, adv, max(0, round(3 * k)) , yo))
    return bytes(bitmap), table, round(H * 1.45)


def main():
    glyphs = extract()
    out = ["// genere par font/make_font.py a partir de font/specimen.png -- ne pas editer a la main",
           "#pragma once", "#include <gfxfont.h>", ""]
    previews = []
    for H in SIZES:
        bm, tb, yadv = build(glyphs, H)
        out.append(f"const uint8_t Chunky{H}Bitmaps[] PROGMEM = {{")
        for i in range(0, len(bm), 16):
            out.append("  " + ",".join(f"0x{b:02X}" for b in bm[i:i + 16]) + ",")
        out.append("};")
        out.append(f"const GFXglyph Chunky{H}Glyphs[] PROGMEM = {{")
        for (bo, w, h, adv, xo, yo) in tb:
            out.append(f"  {{ {bo}, {w}, {h}, {adv}, {xo}, {yo} }},")
        out.append("};")
        out.append(f"const GFXfont Chunky{H} PROGMEM = {{ (uint8_t *)Chunky{H}Bitmaps, (GFXglyph *)Chunky{H}Glyphs, 0x20, 0x7E, {yadv} }};")
        out.append("")
        previews.append((H, bm, tb, yadv))
    (HERE.parent / "font_chunky.h").write_text("\n".join(out), encoding="utf-8")

    # apercu : texte rendu avec les tables generees (verif visuelle)
    text = ["Meat Run  Niveau 12", "Score: 1250  Combo x3 !", "Pieces: 47 (Boutique) ?"]
    W = 640
    canvas = Image.new("RGB", (W, 520), (90, 20, 26))
    px = canvas.load()
    y = 8
    for H, bm, tb, yadv in previews:
        for line in text[: (3 if H <= 22 else 1)]:
            cx, base = 8, y + H
            for ch in line:
                bo, w, h, adv, xo, yo = tb[ord(ch) - 32]
                bit = 0
                for yy in range(h):
                    for xx in range(w):
                        byte = bm[bo + (bit >> 3)]
                        if byte & (0x80 >> (bit & 7)):
                            X, Y = cx + xo + xx, base + yo + yy
                            if 0 <= X < W and 0 <= Y < 520:
                                px[X, Y] = (245, 245, 245)
                        bit += 1
                cx += adv
            y += int(yadv * 0.95)
        y += 6
    canvas.save(HERE / "preview.png")
    print("ok : font_chunky.h + preview.png")


if __name__ == "__main__":
    main()
