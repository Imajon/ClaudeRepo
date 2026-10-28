#!/usr/bin/env python3
"""Convertit la planche d'arbres (assets/arbres.png) en sprites indexes pour le jeu -> tree_sprites.h
Pour chaque arbre : fond gris retire, reduit a la hauteur voulue, palette de <= 24 couleurs (RGB565), octets d'index (0 = transparent).
Usage : python tools_sprites.py [assets/arbres.png]   (les hauteurs cibles sont dans TARGETS)"""
import sys, collections
import numpy as np
from PIL import Image

SRC = sys.argv[1] if len(sys.argv) > 1 else 'assets/arbres.png'
TARGETS = [96, 84, 62]          # hauteurs en pixels du jeu, dans l'ordre : grand chene, arbre mince, petit arbre


def components(mask, min_px):
    H, W = mask.shape
    lab = np.zeros((H, W), int); n = 0; comps = []
    for y in range(H):
        for x in range(W):
            if mask[y, x] and not lab[y, x]:
                n += 1; q = collections.deque([(y, x)]); lab[y, x] = n; pts = []
                while q:
                    cy, cx = q.popleft(); pts.append((cy, cx))
                    for dy in (-1, 0, 1):
                        for dx in (-1, 0, 1):
                            ny, nx = cy + dy, cx + dx
                            if 0 <= ny < H and 0 <= nx < W and mask[ny, nx] and not lab[ny, nx]:
                                lab[ny, nx] = n; q.append((ny, nx))
                if len(pts) >= min_px: comps.append(pts)
    return comps


def rgb565(r, g, b): return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def main():
    im = Image.open(SRC).convert('RGB'); a = np.array(im)
    bg = np.array(collections.Counter(map(tuple, a.reshape(-1, 3))).most_common(1)[0][0])
    mask = np.abs(a.astype(int) - bg).sum(axis=2) > 45
    comps = sorted(components(mask, 400), key=lambda p: -len(p))[:3]
    out = ['// Genere par tools_sprites.py a partir de assets/arbres.png -- ne pas editer a la main.', '#pragma once', '#include <stdint.h>']
    names = []
    for k, pts in enumerate(comps):
        ys = [p[0] for p in pts]; xs = [p[1] for p in pts]
        y0, y1, x0, x1 = min(ys), max(ys), min(xs), max(xs)
        crop = np.zeros((y1 - y0 + 1, x1 - x0 + 1, 4), np.uint8)
        for (y, x) in pts: crop[y - y0, x - x0] = (*a[y, x], 255)
        # comble les petits trous internes (couleur de fond entouree par l'arbre) : ils restent transparents seulement s'ils touchent le bord
        img = Image.fromarray(crop, 'RGBA')
        tgt = TARGETS[k]; w = max(8, round(img.width * tgt / img.height))
        rgb = Image.new('RGB', img.size); rgb.paste(img.convert('RGB'))
        alpha = img.split()[3]
        rgb = rgb.resize((w, tgt), Image.BOX); alpha = alpha.resize((w, tgt), Image.BOX).point(lambda v: 255 if v >= 128 else 0)
        pal_img = rgb.quantize(colors=23, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        pal = pal_img.getpalette()[:23 * 3]; idx = np.array(pal_img).astype(int) + 1
        idx[np.array(alpha) == 0] = 0
        pal565 = [0] + [rgb565(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]) for i in range(23)]
        # ancrage : centre horizontal des pixels de la derniere rangee non transparente (le pied du tronc)
        last = max(r for r in range(tgt) if (idx[r] != 0).any())
        cols = [c for c in range(w) if idx[last, c] != 0]
        ax = int(sum(cols) / len(cols))
        out.append(f'const uint16_t TREE{k}_PAL[24] = {{ ' + ', '.join(f'0x{v:04X}' for v in pal565) + ' };')
        out.append(f'const uint8_t TREE{k}_IDX[{w * tgt}] = {{')
        for r in range(tgt): out.append('  ' + ','.join(str(int(v)) for v in idx[r]) + ',')
        out.append('};')
        out.append(f'#define TREE{k}_W {w}\n#define TREE{k}_H {tgt}\n#define TREE{k}_AX {ax}   // colonne du pied du tronc ; la derniere rangee est le sol')
        names.append(k)
        Image.fromarray(np.dstack([np.array(pal_img.convert('RGB')), np.array(alpha)]), 'RGBA').resize((w * 4, tgt * 4), Image.NEAREST).save(f'assets/_apercu_arbre{k}.png')
    open('tree_sprites.h', 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    print('arbres :', [(len(c)) for c in comps], '-> tree_sprites.h')


main()
