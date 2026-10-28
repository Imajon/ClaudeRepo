#!/usr/bin/env python3
"""assets/samourai5.gif (planche animee : 5 samourais a chapeau, pixel art agrandi x4, 16 images de 100 ms) -> sam5_sprites.h.
On revient aux vrais pixels (blocs 4x4), on isole 5 pistes (zones fixes), on les recadre sur l'ancre des pieds et on garde 5 animations :
repos (garde), course (fente), coup (arcs blancs), saut (elan), touche (flash blanc puis a genoux).
Format PAnim comme perso_sprites.h (4 bits/pixel, 0 = transparent, palette de 15 couleurs).   Apercu : assets/_sam5_apercu.png
Usage : python tools_sam5.py"""
import numpy as np
from PIL import Image

BG = (231, 204, 161)
# (nom, zone (x0, y0, x1, y1) en pixels reels, images du GIF, ancre : 'ref' = image de reference fixe / 'frame' = pieds de chaque image, image de reference)
ANIMS = [('REPOS', (24, 2, 68, 62), list(range(16)), 'ref', 0),
         ('COURSE', (140, 4, 194, 62), list(range(16)), 'ref', 0),
         ('COUP', (78, 64, 210, 136), list(range(3, 14)), 'ref', 3),
         ('SAUT', (18, 110, 70, 208), list(range(3, 8)), 'frame', 0),
         ('TOUCHE', (138, 148, 198, 208), list(range(0, 9)), 'ref', 8)]


def main():
    im = Image.open('assets/samourai5.gif'); fr = []
    for i in range(im.n_frames):
        im.seek(i); fr.append(np.array(im.convert('RGB'))[1::4, 1::4].astype(int))
    cnt = {}
    for f in fr:
        for c in map(tuple, f.reshape(-1, 3)):
            if c != BG: cnt[c] = cnt.get(c, 0) + 1
    pal = [c for c, _ in sorted(cnt.items(), key=lambda kv: -kv[1])[:15]]
    near = {}
    def idx(c):
        if c == BG: return 0
        if c not in near:
            near[c] = 1 + min(range(len(pal)), key=lambda k: sum((a - b) ** 2 for a, b in zip(c, pal[k])))
        return near[c]
    q = [np.array([[idx(tuple(p)) for p in row] for row in f]) for f in fr]
    WHITE = 1 + pal.index((255, 255, 255)) if (255, 255, 255) in pal else -1
    out_pix = bytearray(); table = []; prev = []
    for name, (x0, y0, x1, y1), frames, mode, ref in ANIMS:
        crops = [q[i][y0:y1, x0:x1] for i in frames]
        def anchor(c):                                   # pieds : ligne la plus basse hors blanc, abscisse moyenne des 6 lignes du bas
            m = (c > 0) & (c != WHITE)
            if not m.any(): m = c > 0
            ys, xs = np.nonzero(m); yb = ys.max(); sel = ys >= yb - 5
            return int(round(xs[sel].mean())), int(yb)
        anc = [anchor(c) for c in crops] if mode == 'frame' else [anchor(crops[frames.index(ref)])] * len(crops)
        left = right = up = down = 0
        for c, (ax, ay) in zip(crops, anc):
            ys, xs = np.nonzero(c > 0)
            left = max(left, ax - xs.min()); right = max(right, xs.max() - ax); up = max(up, ay - ys.min()); down = max(down, ys.max() - ay)
        W, H = left + right + 1, up + down + 1; AX, AY = left, up
        print(name, 'images', len(crops), 'taille', W, H, 'ancre', AX, AY)
        off = len(out_pix)
        for c, (ax, ay) in zip(crops, anc):
            can = np.zeros((H, W), int)
            for y, x in zip(*np.nonzero(c > 0)): can[AY + y - ay, AX + x - ax] = c[y, x]
            flat = can.flatten()
            if len(flat) % 2: flat = np.append(flat, 0)
            for i in range(0, len(flat), 2): out_pix.append(int(flat[i]) << 4 | int(flat[i + 1]))
            prev.append((name, can))
        table.append((len(crops), W, H, AX, AY, off))
    rgb565 = lambda c: ((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3)
    o = ['// Genere par tools_sam5.py a partir de assets/samourai5.gif -- ne pas editer a la main.', '#pragma once', '#include "perso_sprites.h"   // PAnim',
         'static const uint16_t SAM5_PAL[16] = { 0, %s };' % ', '.join('0x%04X' % rgb565(c) for c in pal),
         '#define S5_IDLE 0', '#define S5_RUN 1', '#define S5_ATK 2', '#define S5_JUMP 3', '#define S5_HURT 4',
         'static const PAnim SAM5_ANIM[5] = {   // regarde a droite']
    for n, W, H, AX, AY, off in table: o.append('  { 0, %d, %d, %d, %d, %d, 0, %du },' % (n, W, H, AX, AY, off))
    o.append('};')
    o.append('static const uint8_t SAM5_PIX[%d] = {' % len(out_pix))
    for i in range(0, len(out_pix), 32): o.append('  ' + ','.join(str(v) for v in out_pix[i:i + 32]) + ',')
    o.append('};')
    open('sam5_sprites.h', 'w', encoding='utf-8').write('\n'.join(o) + '\n')
    print('sam5_sprites.h', len(out_pix), 'octets')
    colors = [(0, 0, 0)] + pal
    Z = 2; rows = {}
    for name, can in prev: rows.setdefault(name, []).append(can)
    Hh = max(c[0].shape[0] for c in rows.values()); Wm = max(len(v) * v[0].shape[1] for v in rows.values())
    sheet = np.full((len(rows) * (Hh + 2) * Z, Wm * Z, 3), 120, np.uint8)
    for r, (name, lst) in enumerate(rows.items()):
        x = 0
        for can in lst:
            for y in range(can.shape[0]):
                for xx in range(can.shape[1]):
                    if can[y, xx]: sheet[(r * (Hh + 2) + y) * Z:(r * (Hh + 2) + y + 1) * Z, (x + xx) * Z:(x + xx + 1) * Z] = colors[can[y, xx]]
            x += can.shape[1]
    Image.fromarray(sheet).save('assets/_sam5_apercu.png')


if __name__ == '__main__':
    main()
