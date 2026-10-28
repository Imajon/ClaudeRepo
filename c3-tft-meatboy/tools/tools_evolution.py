#!/usr/bin/env python3
"""Planche de l'evolution de l'avatar (copie du dessin du jeu : flameStaged / flameShape / dsFlame) + pistes d'evolution.
Usage : python tools_evolution.py  -> evolution-flamme.png"""
import math
from PIL import Image, ImageDraw, ImageFont

S = 6                                   # agrandissement (pixels du jeu -> pixels de l'image)
CW, CH = 44, 52                         # case d'un stade, en pixels du jeu
BG, PANEL = (14, 14, 22), (26, 26, 38)
O, M, C, E = (30, 85, 230), (80, 170, 255), (225, 246, 255), (8, 24, 100)
TRAIL = (255, 150, 50)
MULTI = [(255, 150, 50), (255, 60, 200), (60, 230, 255), (255, 230, 70)]


def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def flame_shape(d, cx, by, h, wr, sway, cO, cM, cC, t=0.0):
    for L in range(3):
        k = (1.0, 0.68, 0.38)[L]; c = (cO, cM, cC)[L]
        r = int(wr * k + 0.5); cy0 = by - r - L; tip = by - int(h * k); tipx = cx + int(sway * k)
        d.ellipse([cx - r, cy0 - r, cx + r, cy0 + r], fill=c)
        d.polygon([(cx - r, cy0), (cx + r, cy0), (tipx, tip)], fill=c)
        if L == 0:
            d.polygon([(cx - r, cy0 - 1), (cx - r // 2, cy0 - r), (cx - r - 1 + int(math.sin(t * 7) * wr * 0.3), by - int(h * 0.62))], fill=c)
            d.polygon([(cx + r, cy0 - 1), (cx + r // 2, cy0 - r), (cx + r + 1 + int(math.sin(t * 6 + 2) * wr * 0.3), by - int(h * 0.5))], fill=c)


def eyes(d, cx, ey):
    d.rectangle([cx - 2, ey, cx - 1, ey + 2], fill=E); d.rectangle([cx + 2, ey, cx + 3, ey + 2], fill=E)


BC, BR = [1, 0, 2, 1, 0, 2, 1, 1], [0, 0, 0, 1, 1, 1, 2, 3]


def stage(evo):
    im = Image.new('RGB', (CW, CH), PANEL); d = ImageDraw.Draw(im)
    cx, by = CW // 2 + 4, CH - 6
    d.line([(2, by + 1), (CW - 3, by + 1)], fill=(60, 60, 80))
    if evo >= 10:                                           # trainee derriere (vers la gauche)
        for i in range(9):
            c = MULTI[i % 4] if evo >= 11 else TRAIL
            x = cx - 10 - i * 4; y = by - 5 - (i * 5) % 7
            s = max(2, 5 - i // 2)                          # gros points qui retrecissent (comme en jeu)
            d.rectangle([x, y, x + s - 1, y + s - 1], fill=c)
    if evo < 12:
        n = min(evo + 1, 8); b = 4; x0 = cx - b * 3 // 2
        for i in range(n):
            c = O
            if evo >= 8 and BC[i] == 1 and BR[i] >= 1: c = M
            if evo >= 9 and BC[i] == 1 and BR[i] == 1: c = C
            d.rectangle([x0 + BC[i] * b, by - (BR[i] + 1) * b + 1, x0 + BC[i] * b + b - 1, by - BR[i] * b], fill=c)
        rows = 1 if n <= 3 else 2 if n <= 6 else 3 if n == 7 else 4
        top = by - rows * b + 1
        if n <= 3: d.point((cx - 1, top + 1), fill=E); d.point((cx + 1, top + 1), fill=E)
        else: eyes(d, cx, top + 1)
    else:
        es = [0.65, 0.8, 1.0, 1.15, 1.3][min(evo, 16) - 12]
        h, wr = 21 * es, 6.5 * es
        flame_shape(d, cx, by, h, wr, 1.5, O, M, C)
        eyes(d, cx, by - int(wr * 1.3))
    return im


NAMES = ['1 pixel', '2 pixels', '3 pixels', '4 pixels', '5 pixels', '6 pixels', '7 pixels', '8 pixels',
         'couleurs', '+ couleurs', 'trainee', 'trainee multi', 'flamme', 'flamme +', 'flamme ++', 'flamme +++', 'flamme max']


# ---- pistes : ce que l'on pourrait ajouter ----
def idea(kind):
    im = Image.new('RGB', (CW, CH), PANEL); d = ImageDraw.Draw(im)
    cx, by = CW // 2, CH - 6
    d.line([(2, by + 1), (CW - 3, by + 1)], fill=(60, 60, 80))
    wr, h = 7.5, 24
    cols = (O, M, C)
    if kind == 'temperature':                               # la couleur monte en temperature : bleu -> vert -> orange -> rouge -> blanc
        pal = [((40, 170, 80), (120, 230, 120)), ((240, 130, 30), (255, 200, 80)), ((220, 40, 40), (255, 140, 90)), ((235, 235, 255), (255, 255, 255))]
        for j, (a, bcol) in enumerate(pal):
            x = 6 + j * 10; flame_shape(d, x + 3, by, 13, 3.6, 0.5, a, bcol, (255, 255, 240))
        return im
    if kind == 'aura':
        for r, c in ((17, (40, 50, 90)), (14, (50, 70, 130)), (11, (60, 95, 170))):
            d.ellipse([cx - r, by - 14 - r, cx + r, by - 14 + r], outline=c)
    if kind == 'etincelles':
        for (x, y, c) in ((-11, -22, (255, 230, 120)), (9, -27, (255, 255, 255)), (12, -12, (150, 210, 255)), (-8, -31, (255, 200, 80)), (3, -36, (255, 255, 255))):
            d.rectangle([cx + x, by + y, cx + x + 1, by + y + 1], fill=c)
    if kind == 'ailes':
        W1, W2 = (255, 140, 40), (255, 210, 90)
        d.polygon([(cx - 4, by - 14), (cx - 20, by - 26), (cx - 14, by - 12)], fill=W1); d.polygon([(cx - 5, by - 13), (cx - 16, by - 20), (cx - 11, by - 11)], fill=W2)
        d.polygon([(cx + 4, by - 14), (cx + 20, by - 26), (cx + 14, by - 12)], fill=W1); d.polygon([(cx + 5, by - 13), (cx + 16, by - 20), (cx + 11, by - 11)], fill=W2)
    if kind == 'phenix':
        cols = ((240, 90, 30), (255, 180, 60), (255, 250, 210))
        W1, W2 = (220, 50, 40), (255, 150, 40)
        d.polygon([(cx - 4, by - 15), (cx - 21, by - 30), (cx - 17, by - 18), (cx - 22, by - 16), (cx - 12, by - 10)], fill=W1)
        d.polygon([(cx + 4, by - 15), (cx + 21, by - 30), (cx + 17, by - 18), (cx + 22, by - 16), (cx + 12, by - 10)], fill=W1)
        d.polygon([(cx - 6, by - 14), (cx - 15, by - 23), (cx - 10, by - 11)], fill=W2); d.polygon([(cx + 6, by - 14), (cx + 15, by - 23), (cx + 10, by - 11)], fill=W2)
        d.polygon([(cx - 3, by), (cx - 12, by + 4), (cx - 2, by - 3)], fill=W2)
    if kind == 'couronne':
        flame_shape(d, cx, by, h, wr, 1.5, *cols)
        for dx in (-5, 0, 5):
            d.polygon([(cx + dx - 2, by - 22), (cx + dx + 2, by - 22), (cx + dx, by - 29 - (3 if dx == 0 else 0))], fill=(255, 220, 70))
        eyes(d, cx, by - int(wr * 1.3)); return im
    if kind == 'double':
        flame_shape(d, cx - 4, by, h * 0.8, wr * 0.75, -2, *cols); flame_shape(d, cx + 4, by, h, wr * 0.8, 2, *cols)
        eyes(d, cx, by - int(wr * 1.3)); return im
    if kind == 'reflet':
        for i, c in enumerate(((40, 70, 150), (60, 100, 190), (90, 140, 230))):
            w = 14 - i * 4; d.ellipse([cx - w, by - 1, cx + w, by + 2], fill=c)
    flame_shape(d, cx, by, h, wr, 1.5, *cols)
    eyes(d, cx, by - int(wr * 1.3))
    return im


IDEAS = [('temperature', 'couleur = chaleur'), ('aura', 'halo lumineux'), ('etincelles', 'etincelles'), ('reflet', 'eclaire le sol'),
         ('couronne', 'couronne de feu'), ('double', 'double flamme'), ('ailes', 'ailes de feu'), ('phenix', 'phenix (final)')]


def main():
    cols = 9; gap = 4
    cell = (CW * S, CH * S); lab = 30
    try: font = ImageFont.truetype('arial.ttf', 22); big = ImageFont.truetype('arialbd.ttf', 30)
    except OSError: font = big = ImageFont.load_default()
    rows_now = math.ceil(17 / cols)
    W = cols * (cell[0] + gap) + gap
    H = 60 + rows_now * (cell[1] + lab + gap) + 70 + (cell[1] + lab + gap) + 20
    out = Image.new('RGB', (W, H), BG); d = ImageDraw.Draw(out)
    d.text((gap + 6, 14), "Evolution actuelle (1 bonne reponse a une etoile bleue = 1 stade)", font=big, fill=(230, 230, 240))
    y0 = 60
    for e in range(17):
        r, c = divmod(e, cols); x = gap + c * (cell[0] + gap); y = y0 + r * (cell[1] + lab + gap)
        out.paste(stage(e).resize(cell, Image.NEAREST), (x, y))
        d.text((x + 6, y + cell[1] + 2), f"{e}. {NAMES[e]}", font=font, fill=(200, 200, 215))
    y1 = y0 + rows_now * (cell[1] + lab + gap) + 14
    d.text((gap + 6, y1), "Pistes pour la suite (a ajouter apres 'flamme max', ou a intercaler)", font=big, fill=(255, 210, 120))
    y1 += 46
    for i, (k, name) in enumerate(IDEAS):
        x = gap + i * (cell[0] + gap)
        out.paste(idea(k).resize(cell, Image.NEAREST), (x, y1))
        d.text((x + 6, y1 + cell[1] + 2), name, font=font, fill=(255, 220, 160))
    out.save('evolution-flamme.png')
    print('evolution-flamme.png', out.size)


if __name__ == '__main__':
    main()
