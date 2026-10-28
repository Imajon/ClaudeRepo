#!/usr/bin/env python3
"""Convertit assets/perso.gif (planche animee : heroine rouge + fantome violet, 40 images a 70 ms) en sprites -> perso_sprites.h
Le GIF est agrandi x3 et tramé : on revient aux vrais pixels (blocs 3x3, couleur moyenne des pixels non noirs),
puis palette de 15 couleurs par personnage (4 bits par pixel, 0 = transparent).
Usage : python tools_perso.py   (apercu : assets/_perso_apercu.png)"""
import numpy as np
from PIL import Image

SRC = 'assets/perso.gif'
B = 3                                                    # taille d'un pixel de la planche dans le GIF
# (nom, personnage, boite de la cellule a l'image 0 (x0, y0, x1, y1), marge (x, y), regarde a droite ?, coupe a la disparition ?)
ANIMS = [
    ('P_IDLE', 0, (29, 197, 59, 275), (8, 8), True, False),
    ('P_RUN', 0, (300, 307, 353, 365), (8, 8), True, False),
    ('P_JUMP', 0, (262, 197, 309, 275), (8, 8), True, False),
    ('P_ATK', 0, (516, 197, 563, 275), (44, 8), False, True),
    ('P_DIE', 0, (428, 307, 471, 365), (10, 8), True, True),
    ('E_IDLE', 1, (91, 635, 143, 691), (8, 8), True, False),
    ('E_WALK', 1, (301, 635, 357, 691), (8, 8), True, False),
    ('E_ATK', 1, (513, 520, 563, 604), (44, 8), False, True),
    ('E_DIE', 1, (421, 635, 471, 691), (10, 8), True, True),
]


BG = [(36, 36, 0), (0, 0, 0), (0, 0, 85), (36, 36, 85)]   # les 4 couleurs tramees du fond
BGAVG = np.array([22.5, 22.5, 22.8])                       # leur moyenne (fond vu de loin)


def fg(a):
    m = np.ones(a.shape[:2], bool)
    for c in BG: m &= ~((a[:, :, 0] == c[0]) & (a[:, :, 1] == c[1]) & (a[:, :, 2] == c[2]))
    return m


def kmeans(X, k, it=25):
    rng = np.random.default_rng(1)
    C = X[rng.choice(len(X), k, replace=False)].astype(float)
    for _ in range(it):
        d = ((X[:, None, :] - C[None]) ** 2).sum(2); lab = d.argmin(1)
        for j in range(k):
            if (lab == j).any(): C[j] = X[lab == j].mean(0)
    return C


def main():
    im = Image.open(SRC); frames = []
    for i in range(im.n_frames): im.seek(i); frames.append(np.array(im.convert('RGB')).astype(int))
    H, W = frames[0].shape[:2]
    m0 = fg(frames[0])
    best = None                                          # decalage de la grille 3x3 : celui ou les blocs sont les plus uniformes
    for oy in range(B):
        for ox in range(B):
            sub = m0[oy:oy + (H - oy) // B * B, ox:ox + (W - ox) // B * B]
            blk = sub.reshape(sub.shape[0] // B, B, sub.shape[1] // B, B).sum((1, 3))
            score = ((blk == 0) | (blk == B * B)).sum()
            if best is None or score > best[0]: best = (score, ox, oy)
    _, OX, OY = best
    print('grille', OX, OY)

    def blocks(f, x0, y0, x1, y1):                       # (rgb moyen, opaque) par bloc dans la zone
        bx0, by0 = (x0 - OX) // B, (y0 - OY) // B; bx1, by1 = (x1 - OX) // B + 1, (y1 - OY) // B + 1
        rgb = np.zeros((by1 - by0, bx1 - bx0, 3)); op = np.zeros((by1 - by0, bx1 - bx0), bool)
        for j in range(by1 - by0):
            for i in range(bx1 - bx0):
                px, py = OX + (bx0 + i) * B, OY + (by0 + j) * B
                if px < 0 or py < 0 or px + B > W or py + B > H: continue
                a = f[py:py + B, px:px + B].reshape(-1, 3); avg = a.mean(0)   # le tramage se lit en moyenne sur le bloc
                if (a == 0).all(): op[j, i] = True; rgb[j, i] = (10, 8, 20)          # bloc tout noir : le noir du personnage (le fond n'est jamais tout noir)
                elif np.abs(avg - BGAVG).sum() > 40: op[j, i] = True; rgb[j, i] = avg
        return rgb, op, bx0, by0

    out = {}
    for name, ch, (x0, y0, x1, y1), (MX, MY), right, cut in ANIMS:
        seq = []
        ref = None
        for k in range(len(frames)):
            rgb, op, bx0, by0 = blocks(frames[k], x0 - MX, y0 - MY, x1 + MX, y1 + MY)
            n = op.sum()
            if ref is None: ref = n
            if cut and n < ref * 0.5: break                   # le personnage disparait : fin de l'animation
            if seq and (op == seq[-1][1]).all() and np.abs(rgb - seq[-1][0]).max() < 8: continue   # image identique a la precedente
            seq.append((rgb, op))
        ys, xs = np.where(np.any([s[1] for s in seq], 0))
        cy0, cy1, cx0, cx1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        ax = ((x0 + x1) / 2 - OX) / B - bx0 - cx0          # ancre : milieu de la cellule, bas de la cellule (les pieds)
        ay = (y1 - OY) / B - by0 - cy0 + 1
        out[name] = (ch, [(r[cy0:cy1, cx0:cx1], o[cy0:cy1, cx0:cx1]) for r, o in seq], int(round(ax)), int(round(ay)), right)
        print(name, len(seq), 'images', cx1 - cx0, 'x', cy1 - cy0, 'ancre', int(round(ax)), int(round(ay)))

    pals = []
    for ch in (0, 1):
        X = np.concatenate([r[o] for n, (c, seq, *_) in out.items() if c == ch for r, o in seq])
        pals.append(kmeans(X, 15))
    lines = ['// Genere par tools_perso.py a partir de assets/perso.gif -- ne pas editer a la main.', '#pragma once', '#include <stdint.h>',
             'struct PAnim { uint8_t ch, n, w, h; int8_t ax, ay; uint8_t right; uint32_t off; };   // personnage, images, taille (pixels de la planche), ancre (pieds), regarde a droite, debut dans PERSO_PIX',
             'const uint16_t PERSO_PAL[2][16] = {']
    for P in pals:
        lines.append('  { 0, ' + ', '.join('0x%04X' % (((int(c[0]) >> 3) << 11) | ((int(c[1]) >> 2) << 5) | (int(c[2]) >> 3)) for c in P) + ' },')
    lines.append('};')
    pix = []; table = []
    for name, (ch, seq, ax, ay, right) in out.items():
        h, w = seq[0][1].shape
        table.append((name, ch, len(seq), w, h, ax, ay, right, len(pix)))
        for r, o in seq:
            d = ((r[:, :, None, :] - pals[ch][None, None]) ** 2).sum(3).argmin(2) + 1
            d[~o] = 0
            flat = d.flatten().tolist()
            if len(flat) % 2: flat.append(0)
            pix += [(flat[i] << 4) | flat[i + 1] for i in range(0, len(flat), 2)]
    lines.append('const uint8_t PERSO_PIX[%d] = {' % len(pix))
    for i in range(0, len(pix), 32): lines.append('  ' + ','.join(str(v) for v in pix[i:i + 32]) + ',')
    lines.append('};')
    for i, t in enumerate(table): lines.append('#define %s %d' % (t[0], i))
    lines.append('const PAnim PERSO_ANIM[%d] = {' % len(table))
    for t in table: lines.append('  { %d, %d, %d, %d, %d, %d, %d, %du },   // %s' % (t[1], t[2], t[3], t[4], t[5], t[6], 1 if t[7] else 0, t[8], t[0]))
    lines.append('};')
    open('perso_sprites.h', 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
    print('perso_sprites.h', len(pix), 'octets de pixels')

    # apercu : chaque animation sur une ligne, x3
    rows = []
    for name, (ch, seq, ax, ay, right) in out.items():
        h, w = seq[0][1].shape; strip = Image.new('RGB', (max(1, len(seq)) * (w + 2) * 3, h * 3 + 6), (70, 70, 80))
        for k, (r, o) in enumerate(seq):
            d = ((r[:, :, None, :] - pals[ch][None, None]) ** 2).sum(3).argmin(2)
            img = np.where(o[:, :, None], pals[ch][d], [[70, 70, 80]]).astype(np.uint8)
            strip.paste(Image.fromarray(img).resize((w * 3, h * 3), Image.NEAREST), (k * (w + 2) * 3, 3))
        rows.append(strip)
    Wd = max(r.width for r in rows); Hd = sum(r.height for r in rows)
    sheet = Image.new('RGB', (min(Wd, 3000), Hd), (40, 40, 50)); y = 0
    for r in rows: sheet.paste(r, (0, y)); y += r.height
    sheet.save('assets/_perso_apercu.png')


if __name__ == '__main__':
    main()
