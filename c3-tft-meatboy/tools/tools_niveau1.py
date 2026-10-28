#!/usr/bin/env python3
"""assets/niveau1.png (decor de grotte du niveau 1 du Samourai, 498 x 648, niveaux de gris) -> niveau1_bg.h : le decor est repris tel quel, a l'echelle 1.
On retire d'abord les annotations du croquis (cadre blanc du depart, fleches, ligne en pointilles : pixels nettement plus clairs que leur voisinage)
et le balancier a pointes (il est dessine par le jeu, qui le fait osciller), puis on ramene l'image a 16 gris (kmeans 1D, sans trame : affichage a 300 %), 4 bits/pixel.
Apercu : assets/_niveau1_apercu.png            Usage : python tools_niveau1.py"""
import numpy as np
from PIL import Image, ImageFilter


def main():
    im = Image.open('assets/niveau1.png').convert('L'); a = np.array(im).astype(float); H, W = a.shape
    med = np.array(im.filter(ImageFilter.MedianFilter(11))).astype(float)
    ann = np.zeros((H, W), bool)                                       # annotations du croquis : cadre du depart, fleches, ligne en pointilles (zones fixes, pixels plus clairs que leur voisinage)
    for x0, y0, x1, y1 in ((106, 248, 128, 276), (144, 252, 164, 280), (176, 250, 198, 278), (284, 124, 310, 168), (436, 14, 468, 46), (350, 172, 376, 192), (396, 172, 424, 192), (306, 234, 468, 250), (178, 12, 318, 92)):
        r = (a[y0:y1, x0:x1] - med[y0:y1, x0:x1]) > 8
        ann[y0:y1, x0:x1] |= r
    for _ in range(2):                                                 # on elargit de 2 pixels (bords adoucis)
        ann |= np.pad(ann[:-1, :], ((1, 0), (0, 0))) | np.pad(ann[1:, :], ((0, 1), (0, 0))) | np.pad(ann[:, :-1], ((0, 0), (1, 0))) | np.pad(ann[:, 1:], ((0, 0), (0, 1)))
    a = np.where(ann, med, a)
    for x0, y0, x1, y1 in ((48, 331, 132, 392), (238, 42, 261, 71)):  # balancier a pointes (dessine par le jeu) et petit personnage du croquis : on interpole le gris a gauche / a droite, ligne par ligne
        for y in range(y0, y1):
            l = a[y, x0 - 8:x0 - 2]; r = a[y, x1 + 2:x1 + 8]
            lv = np.median(l[l > 20]) if (l > 20).any() else 50.0; rv = np.median(r[r > 20]) if (r > 20).any() else lv
            a[y, x0:x1] = np.linspace(lv, rv, x1 - x0)
    print('annotations retirees :', int(ann.sum()), 'pixels')
    v = a.flatten(); v1 = v[(v > 0.5)]
    C = np.linspace(v1.min(), v1.max(), 15)                            # kmeans 1D sur les gris non noirs (le noir pur reste l'indice 0)
    for _ in range(40):
        lab = np.abs(v1[:, None] - C[None]).argmin(1)
        for k in range(15):
            if (lab == k).any(): C[k] = v1[lab == k].mean()
    pal = np.concatenate([[0.0], np.sort(C)])
    print('gris :', [int(round(c)) for c in pal])
    out = np.where(a < 0.5, 0, np.abs(a[..., None] - pal[None, None, 1:]).argmin(2) + 1).astype(np.uint8)   # pas de trame : le decor est affiche a 300 %, une trame agrandie serait du bruit (les noirs purs restent l'indice 0)
    assert W % 2 == 0
    pix = (out[:, 0::2].astype(np.uint8) << 4) | out[:, 1::2]
    rgb565 = lambda g: ((int(g) >> 3) << 11) | ((int(g) >> 2) << 5) | (int(g) >> 3)
    o = ['// Genere par tools_niveau1.py a partir de assets/niveau1.png -- ne pas editer a la main.', '#pragma once', '#include <stdint.h>',
         '#define NIV1_W %d' % W, '#define NIV1_H %d' % H,
         'static const uint16_t NIV1_PAL[16] = { %s };' % ', '.join('0x%04X' % rgb565(g) for g in pal),
         'static const uint8_t NIV1_PIX[%d] = {' % pix.size]
    flat = pix.flatten()
    for i in range(0, len(flat), 40): o.append('  ' + ','.join(str(int(x)) for x in flat[i:i + 40]) + ',')
    o.append('};')
    open('niveau1_bg.h', 'w', encoding='utf-8').write('\n'.join(o) + '\n')
    print('niveau1_bg.h', pix.size, 'octets')
    Image.fromarray(np.array([pal[k] for k in out.flatten()]).reshape(H, W).astype(np.uint8)).save('assets/_niveau1_apercu.png')


if __name__ == '__main__':
    main()
