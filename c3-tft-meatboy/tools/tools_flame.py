#!/usr/bin/env python3
"""Convertit la planche de flammes (assets/flame.png : 7 poses) en sprites animes recolorables -> flame_sprites.h
Chaque pixel est un niveau de luminosite 1..7 (0 = transparent) ; le jeu choisit la couleur a l'affichage (couleur du palier du joueur)
en projetant les niveaux sur une rampe sombre -> couleur -> coeur presque blanc.
Usage : python tools_flame.py"""
import numpy as np
from PIL import Image

SRC, HM = 'assets/flame.png', 72           # hauteur maitre (px) de la flamme la plus haute


def main():
    im = Image.open(SRC).convert('RGBA'); a = np.array(im)
    opaque = a[:, :, 3] > 40
    cols = opaque.any(axis=0)
    frames = []; start = None; gap = 0
    for x in range(a.shape[1] + 1):
        on = x < a.shape[1] and cols[x]
        if on:
            if start is None: start = x
            gap = 0; last = x
        elif start is not None:
            gap += 1
            if gap >= 10 or x >= a.shape[1]: frames.append((start, last)); start = None; gap = 0
    crops = []
    for (x0, x1) in frames:
        sub = a[:, x0:x1 + 1]; rows = np.where((sub[:, :, 3] > 40).any(axis=1))[0]
        crops.append(sub[rows.min():rows.max() + 1])
    H0 = max(c.shape[0] for c in crops); s = HM / H0
    prepared = []
    for c in crops:
        h = max(4, round(c.shape[0] * s)); w = max(4, round(c.shape[1] * s))
        al = c[:, :, 3].astype(np.float32) / 255.0
        ch = [np.array(Image.fromarray(c[:, :, k].astype(np.float32) * al).resize((w, h), Image.BOX)) for k in range(3)]
        alr = np.array(Image.fromarray(al).resize((w, h), Image.BOX))
        rgb = np.stack([np.where(alr > 0.01, x / np.maximum(alr, 0.01), 0) for x in ch], axis=2)
        solid = alr >= 0.45
        # ancrage : centre horizontal des pixels du bas (le pied de la flamme)
        bottom = np.where(solid[int(h * 0.82):].any(axis=0))[0]
        prepared.append((rgb, solid, int(bottom.mean()) if len(bottom) else w // 2))
    left = max(p[2] for p in prepared); right = max(p[0].shape[1] - p[2] for p in prepared)
    W = left + right
    out = ['// Genere par tools_flame.py a partir de assets/flame.png -- ne pas editer a la main.', '#pragma once', '#include <stdint.h>',
           f'#define FLAME_N {len(prepared)}', f'#define FLAME_W {W}', f'#define FLAME_H {HM}', f'const uint8_t FLAME_IDX[{len(prepared) * W * HM}] = {{']
    prev = []
    for (rgb, solid, ax) in prepared:
        canvas = np.zeros((HM, W), np.uint8)
        L = 0.299 * rgb[:, :, 0] + 0.587 * rgb[:, :, 1] + 0.114 * rgb[:, :, 2]
        lv = (1 + np.clip(((L - 70) / (245 - 70)) * 7, 0, 6.99)).astype(np.uint8)
        lv[~solid] = 0
        h, w = lv.shape
        canvas[HM - h:, left - ax:left - ax + w] = lv
        for r in range(HM): out.append('  ' + ','.join(str(int(v)) for v in canvas[r]) + ',')
        prev.append(canvas)
    out.append('};')
    open('flame_sprites.h', 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    # apercu : les poses recolorees avec la rampe du palier "or" (orange) et du palier "diamant" (rose)
    def ramp(base, dark):
        mix = lambda p, q, t: tuple(int(p[i] + (q[i] - p[i]) * t / 256) for i in range(3))
        return [(0, 0, 0), mix(dark, (0, 0, 0), 110), dark, mix(dark, base, 128), base, mix(base, (255, 255, 255), 70), mix(base, (255, 255, 255), 140), mix(base, (255, 250, 230), 215)]
    sheet = Image.new('RGB', (len(prev) * (W + 6) * 3, HM * 3 * 2), (60, 70, 100))
    for row, (base, dark) in enumerate([((255, 157, 77), (196, 106, 28)), ((255, 143, 192), (192, 74, 140))]):
        rp = ramp(base, dark)
        for i, cv in enumerate(prev):
            img = Image.new('RGB', (W, HM), (60, 70, 100)); px = img.load()
            for y in range(HM):
                for x in range(W):
                    if cv[y, x]: px[x, y] = rp[cv[y, x]]
            sheet.paste(img.resize((W * 3, HM * 3), Image.NEAREST), (i * (W + 6) * 3, row * HM * 3))
    sheet.save('assets/_test_flame.png')
    print('flammes :', len(prepared), 'poses', W, 'x', HM, '-> flame_sprites.h')


main()
