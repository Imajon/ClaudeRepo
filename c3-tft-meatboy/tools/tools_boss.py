#!/usr/bin/env python3
"""assets/starWars.gif (guerrier encapuchonne au sabre rouge, 18 images : garde 0-2, coup 3-10, garde 11-17) -> boss_sprites.h : petit boss de fin du Samourai.
Le fond uni du GIF est retire, les images sont reduites (ECH), la palette est de 15 couleurs (kmeans), 4 bits/pixel (0 = transparent), ancre = pieds.
Il regarde a gauche dans le GIF.   Apercu : assets/_boss_apercu.png      Usage : python tools_boss.py"""
import numpy as np
from PIL import Image
from tools_perso import kmeans

ECH = 0.40


def main():
    im = Image.open('assets/starWars.gif'); fr = []
    for i in range(im.n_frames):
        im.seek(i); fr.append(np.array(im.convert('RGB')).astype(float))
    bg = fr[0][0, 0]; print('fond', bg)
    H, W = fr[0].shape[:2]; h, w = int(round(H * ECH)), int(round(W * ECH))
    sm = []
    for f in fr:
        m = (np.abs(f - bg).sum(2) > 45).astype(float)
        rgb = Image.fromarray((f * m[..., None]).astype(np.uint8)).resize((w, h), Image.LANCZOS)
        al = Image.fromarray((m * 255).astype(np.uint8)).resize((w, h), Image.LANCZOS)
        a = np.array(al).astype(float) / 255; c = np.array(rgb).astype(float) / np.maximum(a[..., None], 1e-3)
        sm.append((a > 0.45, np.clip(c, 0, 255)))
    ys = [np.nonzero(m)[0] for m, _ in sm if m.any()]; xs = [np.nonzero(m)[1] for m, _ in sm if m.any()]
    y0, y1 = min(v.min() for v in ys), max(v.max() for v in ys) + 1; x0, x1 = min(v.min() for v in xs), max(v.max() for v in xs) + 1
    ref = sm[11][0]; ry, rx = np.nonzero(ref); yb = ry.max(); sel = ry >= yb - 5
    axr, ayr = int(round(rx[sel].mean())), int(yb)                    # ancre : pieds du guerrier au repos
    CW, CH = x1 - x0, y1 - y0; AX, AY = axr - x0, ayr - y0
    print('image', CW, CH, 'ancre', AX, AY, 'images', len(sm))
    pix = np.concatenate([c[m] for m, c in sm]); pal = kmeans(pix[::max(1, len(pix) // 6000)], 15).astype(int)
    out = bytearray(); prev = []
    for m, c in sm:
        mm = m[y0:y1, x0:x1]; cc = c[y0:y1, x0:x1]
        idx = np.zeros((CH, CW), int)
        yy, xx = np.nonzero(mm)
        d = ((cc[yy, xx][:, None, :] - pal[None]) ** 2).sum(2); idx[yy, xx] = d.argmin(1) + 1
        flat = idx.flatten()
        if len(flat) % 2: flat = np.append(flat, 0)
        for i in range(0, len(flat), 2): out.append(int(flat[i]) << 4 | int(flat[i + 1]))
        prev.append(idx)
    rgb565 = lambda c: ((int(c[0]) >> 3) << 11) | ((int(c[1]) >> 2) << 5) | (int(c[2]) >> 3)
    o = ['// Genere par tools_boss.py a partir de assets/starWars.gif -- ne pas editer a la main.', '#pragma once',
         '#define BOSS_W %d' % CW, '#define BOSS_H %d' % CH, '#define BOSS_AX %d' % AX, '#define BOSS_AY %d' % AY, '#define BOSS_NF %d' % len(sm),
         '// images : 0-2 garde (prise de l\'elan), 3-10 coup (arcs rouges), 11-17 garde ; il regarde a gauche',
         'static const uint16_t BOSS_PAL[16] = { 0, %s };' % ', '.join('0x%04X' % rgb565(c) for c in pal),
         'static const uint8_t BOSS_PIX[%d] = {' % len(out)]
    for i in range(0, len(out), 32): o.append('  ' + ','.join(str(v) for v in out[i:i + 32]) + ',')
    o.append('};')
    open('boss_sprites.h', 'w', encoding='utf-8').write('\n'.join(o) + '\n')
    print('boss_sprites.h', len(out), 'octets')
    cols = 9; rows = (len(prev) + cols - 1) // cols; Z = 1
    cl = [(0, 0, 0)] + [tuple(int(v) for v in c) for c in pal]
    sheet = np.full((rows * CH, cols * CW, 3), (150, 170, 160), np.uint8)
    for i, idx in enumerate(prev):
        r, c = divmod(i, cols)
        for y, x in zip(*np.nonzero(idx)): sheet[r * CH + y, c * CW + x] = cl[idx[y, x]]
    Image.fromarray(sheet).save('assets/_boss_apercu.png')


if __name__ == '__main__':
    main()
