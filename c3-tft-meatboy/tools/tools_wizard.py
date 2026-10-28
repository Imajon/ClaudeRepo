#!/usr/bin/env python3
"""Convertit assets/wizard1..5 + 7.gif en sprites -> wizard_sprites.h (meme format que perso_sprites.h : PAnim, 4 bits/pixel, 0 = transparent).
Chaque GIF est agrandi (B = taille d'un pixel de dessin) : on revient aux vrais pixels (couleur moyenne du bloc), on recadre sur
l'union de toutes les images, on garde une image sur STEP (duree d'image = duree GIF x STEP), palette de 15 couleurs par sorcier.
wizard1 = ecran titre ; wizard2..5 + 7 = sorciers qui posent les questions en jeu.
Usage : python tools_wizard.py   (apercu : assets/_wizard_apercu.png)"""
import numpy as np
from PIL import Image
from tools_perso import kmeans

# (fichier, taille du pixel de dessin, garder 1 image sur STEP, regarde a droite ?, recadrage autour de l'ancre (gauche, droite, haut) ou None,
#  sequences [(premiere, derniere image du GIF)] : une animation chacune -- ecran titre : tout le GIF ; questions : idle puis attaque)
WIZ = [('assets/wizard1.gif', 3, 4, True, (40, 68, 110), [(0, 173)]),
       ('assets/wizard2.gif', 6, 1, False, None, [(14, 21), (0, 13)]),
       ('assets/wizard3.gif', 6, 1, True, None, [(0, 27), (28, 44)]),
       ('assets/wizard4.gif', 6, 1, True, None, [(0, 15), (16, 37)]),
       ('assets/wizard5.gif', 6, 1, True, None, [(19, 35), (0, 18)]),
       ('assets/wizard7.gif', 5, 1, True, None, [(16, 39), (40, 68)])]


def load(path):
    im = Image.open(path); fr = []
    ds = []
    for i in range(im.n_frames): im.seek(i); fr.append(np.array(im.convert('RGB')).astype(int)); ds.append(im.info.get('duration', 70))
    return fr, int(np.median(ds))


WIZ_PAL_OF = []    # numero de palette de chaque animation
pal_src = []       # images de chaque sorcier (pour sa palette)


def main():
    out = []
    for path, B, STEP, right, crop, segs in WIZ:
        fr, dur = load(path)
        bg = fr[0][0, 0]
        isbg = lambda f: np.abs(f - bg).sum(2) < 24                 # fond uni (wizard1 : gris legerement bruite)
        H, W = fr[0].shape[:2]
        best = None                                                  # decalage de la grille : blocs les plus uniformes
        for oy in range(B):
            for ox in range(B):
                a = fr[0][oy:oy + (H - oy) // B * B, ox:ox + (W - ox) // B * B]
                blk = a.reshape(a.shape[0] // B, B, a.shape[1] // B, B, 3)
                sc = (blk.max((1, 3)) == blk.min((1, 3))).all(-1).sum()
                if best is None or sc > best[0]: best = (sc, ox, oy)
        _, OX, OY = best
        hb, wb = (H - OY) // B, (W - OX) // B
        seq = []; ks = [k for a, b in segs for k in range(a, b + 1, STEP)]
        for k in ks:
            a = fr[k][OY:OY + hb * B, OX:OX + wb * B]
            m = (~isbg(a)).reshape(hb, B, wb, B).mean((1, 3)) > 0.5
            nb = sum(np.roll(np.roll(m, dy, 0), dx, 1) for dy in range(-2, 3) for dx in range(-2, 3))
            m &= nb >= 4                                             # poussieres isolees retirees
            rgb = a.reshape(hb, B, wb, B, 3).mean((1, 3))
            seq.append((rgb, m))
        ys, xs = np.where(np.any([s[1] for s in seq], 0))
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        # ancre : pieds = bas de l'image 0, centre du personnage a l'image 0
        ys0, xs0 = np.where(seq[0][1][y0:y1, x0:x1])
        ax, ay = int(np.median(xs0)), int(ys0.max()) + 1
        if crop:                                                     # ecran titre : on ne garde qu'une fenetre autour du sorcier
            l, r, u = crop; ax0, ay0 = x0 + ax, y0 + ay
            x0, x1, y0, y1 = max(x0, ax0 - l), min(x1, ax0 + r), max(y0, ay0 - u), y1
            ax, ay = ax0 - x0, ay0 - y0
        seq = [(r[y0:y1, x0:x1], m[y0:y1, x0:x1]) for r, m in seq]
        i = 0
        for a, b in segs:                                            # meme cadre et meme ancre pour toutes les sequences d'un sorcier
            n = len(range(a, b + 1, STEP)); out.append((seq[i:i + n], ax, ay, right, dur * STEP, 0)); i += n
            WIZ_PAL_OF.append(len(pal_src))
        pal_src.append(seq)
        print(path, 'grille', OX, OY, len(seq), 'images', x1 - x0, 'x', y1 - y0, 'ancre', ax, ay, dur * STEP, 'ms')

    pals = [kmeans(np.concatenate([r[m] for r, m in seq]), 15) for seq in pal_src]
    L = ['// Genere par tools_wizard.py a partir de assets/wizard1..5 + 7.gif -- ne pas editer a la main.', '#pragma once',
         '#include "perso_sprites.h"   // struct PAnim (ch = numero de palette)', 'const uint16_t WIZ_PAL[%d][16] = {' % len(out)]
    for P in pals:
        L.append('  { 0, ' + ', '.join('0x%04X' % (((int(c[0]) >> 3) << 11) | ((int(c[1]) >> 2) << 5) | (int(c[2]) >> 3)) for c in P) + ' },')
    L.append('};')
    pix = []; table = []
    for w, (seq, ax, ay, right, ms, _) in enumerate(out):
        h, wd = seq[0][1].shape; pl = WIZ_PAL_OF[w]
        table.append((pl, len(seq), wd, h, ax, ay, right, len(pix), ms))
        for r, m in seq:
            d = ((r[:, :, None, :] - pals[pl][None, None]) ** 2).sum(3).argmin(2) + 1
            d[~m] = 0
            flat = d.flatten().tolist()
            if len(flat) % 2: flat.append(0)
            pix += [(flat[i] << 4) | flat[i + 1] for i in range(0, len(flat), 2)]
    L.append('const uint8_t WIZ_PIX[%d] = {' % len(pix))
    for i in range(0, len(pix), 32): L.append('  ' + ','.join(str(v) for v in pix[i:i + 32]) + ',')
    L.append('};')
    L.append('const PAnim WIZ_ANIM[%d] = {' % len(table))
    for t in table: L.append('  { %d, %d, %d, %d, %d, %d, %d, %du },' % t[:8])
    L.append('};')
    L.append('#define WIZ_IDLE(w) (2 * (w) - 1)   // sorcier de question w (1..%d) : animation idle, puis son attaque' % (len(pal_src) - 1))
    L.append('#define WIZ_ATK(w) (2 * (w))')
    L.append('const uint16_t WIZ_MS[%d] = { %s };   // duree d\'une image' % (len(table), ', '.join(str(t[8]) for t in table)))
    open('wizard_sprites.h', 'w', encoding='utf-8').write('\n'.join(L) + '\n')
    print('wizard_sprites.h', len(pix), 'octets de pixels')

    rows = []                                                         # apercu : 12 images par sorcier, x2
    for w, (seq, *_) in enumerate(out):
        pl = WIZ_PAL_OF[w]; h, wd = seq[0][1].shape; pick = seq[::max(1, len(seq) // 12)][:12]
        strip = Image.new('RGB', (len(pick) * (wd + 2) * 2, h * 2 + 4), (70, 70, 80))
        for k, (r, m) in enumerate(pick):
            d = ((r[:, :, None, :] - pals[pl][None, None]) ** 2).sum(3).argmin(2)
            img = np.where(m[:, :, None], pals[pl][d], [[70, 70, 80]]).astype(np.uint8)
            strip.paste(Image.fromarray(img).resize((wd * 2, h * 2), Image.NEAREST), (k * (wd + 2) * 2, 2))
        rows.append(strip)
    sheet = Image.new('RGB', (max(r.width for r in rows), sum(r.height for r in rows)), (40, 40, 50)); y = 0
    for r in rows: sheet.paste(r, (0, y)); y += r.height
    sheet.save('assets/_wizard_apercu.png')


if __name__ == '__main__':
    main()
