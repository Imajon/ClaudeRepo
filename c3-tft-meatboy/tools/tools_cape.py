#!/usr/bin/env python3
"""assets/avatarCape.png (6 poses d'un petit etre noir a cape bleue) -> cape_sprites.h : 6 poses + images intermediaires (morphing de formes) entre
les poses voisines, pour un avatar fluide. Chaque pose = 3 calques (silhouette, cape, yeux) convertis en champs de distance signes ; l'intermediaire
est l'interpolation des champs, seuillee -> les formes coulent d'une pose a l'autre (c'est un etre d'encre : ca lui va bien).
Format : 4 bits/pixel (0 transparent, 1 noir, 2 cape, 3 yeux, 4 liseré clair), memes mots que sprDraw.  Apercu : assets/_cape_apercu.png
Usage : python tools_cape.py"""
import numpy as np
from PIL import Image

SRC = 'assets/avatarCape.png'
TARGET_H = 46            # hauteur d'une image dans le jeu (px)
M = 3                    # images intermediaires par arete
# poses (ordre de la planche : haut gauche, haut milieu, haut droite, bas gauche, bas milieu, bas droite) :
#  0 face, repos   1 marche 3/4 (cape qui vole)   2 saut / elan   3 face, accroupi / attaque (yeux en colere)   4 marche 2 / chute   5 en boule (touche)
POSE_NAMES = ['repos', 'marche A', 'saut', 'accroupi', 'marche B', 'touche']
EDGES = [(0, 1), (1, 4), (1, 2), (2, 4), (4, 3), (3, 0), (3, 5), (0, 2)]
PAL = [(0, 0, 0), (10, 10, 16), (34, 79, 98), (242, 246, 252), (130, 160, 185)]


def label(mask):
    H, W = mask.shape; lab = np.zeros((H, W), int); n = 0
    for y in range(H):
        for x in range(W):
            if mask[y, x] and not lab[y, x]:
                n += 1; st = [(y, x)]; lab[y, x] = n
                while st:
                    cy, cx = st.pop()
                    for dy in (-1, 0, 1):
                        for dx in (-1, 0, 1):
                            yy, xx = cy + dy, cx + dx
                            if 0 <= yy < H and 0 <= xx < W and mask[yy, xx] and not lab[yy, xx]: lab[yy, xx] = n; st.append((yy, xx))
    return lab, n


def outside(S):                                     # fond relie au bord (4 voisins)
    H, W = S.shape; out = np.zeros((H, W), bool); st = []
    for x in range(W):
        for y in (0, H - 1):
            if not S[y, x] and not out[y, x]: out[y, x] = True; st.append((y, x))
    for y in range(H):
        for x in (0, W - 1):
            if not S[y, x] and not out[y, x]: out[y, x] = True; st.append((y, x))
    while st:
        y, x = st.pop()
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            yy, xx = y + dy, x + dx
            if 0 <= yy < H and 0 <= xx < W and not S[yy, xx] and not out[yy, xx]: out[yy, xx] = True; st.append((yy, xx))
    return out


def load_poses():
    a = np.array(Image.open(SRC).convert('RGB')).astype(int); r, g, b = a[..., 0], a[..., 1], a[..., 2]; lum = a.mean(2)
    C = (b - r > 22) & (lum < 175)
    S = (lum < 140) | C
    lab, n = label(S); H, W = S.shape
    cells = {}
    for i in range(1, n + 1):
        ys, xs = np.nonzero(lab == i)
        if len(ys) < 40: continue
        cells.setdefault((int(ys.mean() * 2 // H), int(xs.mean() * 3 // W)), []).append(i)
    holes_all = ~outside(S) & ~S
    hl, hn = label(holes_all)
    poses = []
    for key in sorted(cells):
        sel = np.isin(lab, cells[key]); ys, xs = np.nonzero(sel)
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        box = np.zeros_like(S); box[y0:y1, x0:x1] = True
        E = np.zeros_like(S)
        for i in range(1, hn + 1):
            hm = hl == i
            if hm.sum() >= 15 and (hm & box).sum() == hm.sum(): E |= hm
        Cm = C & sel
        Sil = sel | E
        Bl = Sil & ~Cm & ~E
        bx = np.nonzero(Bl)[1].mean()
        poses.append(dict(Sil=Sil[y0:y1, x0:x1], C=Cm[y0:y1, x0:x1], E=E[y0:y1, x0:x1], ax=bx - x0, ay=float(y1 - y0)))
    assert len(poses) == 6, len(poses)
    return poses


def sdf(mask):                                      # distance signee (>0 dedans), a ~0,5 px pres
    H, W = mask.shape
    p = np.pad(mask, 1)
    edge = mask & ~(p[:-2, 1:-1] & p[2:, 1:-1] & p[1:-1, :-2] & p[1:-1, 2:])
    edge |= ~mask & (p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:])
    ey, ex = np.nonzero(edge)
    if len(ey) == 0: return np.full((H, W), 50.0 if mask.any() else -50.0, np.float32)
    yy, xx = np.mgrid[0:H, 0:W]; d = np.full((H, W), 1e9, np.float32)
    for i in range(0, len(ey), 200):
        dy = yy[..., None] - ey[None, None, i:i + 200]; dx = xx[..., None] - ex[None, None, i:i + 200]
        d = np.minimum(d, np.sqrt(dy * dy + dx * dx).min(2).astype(np.float32))
    return np.where(mask, d, -d)


def bil(f, ys, xs):                                 # echantillonnage bilineaire de f aux coordonnees (ys, xs) (tableaux)
    H, W = f.shape; ys = np.clip(ys, 0, H - 1.001); xs = np.clip(xs, 0, W - 1.001)
    y0 = ys.astype(int); x0 = xs.astype(int); fy = ys - y0; fx = xs - x0
    return f[y0, x0] * (1 - fy) * (1 - fx) + f[y0, x0 + 1] * (1 - fy) * fx + f[y0 + 1, x0] * fy * (1 - fx) + f[y0 + 1, x0 + 1] * fy * fx


def main():
    P = load_poses()
    left = max(p['ax'] for p in P); right = max(p['Sil'].shape[1] - p['ax'] for p in P); up = max(p['ay'] for p in P)
    CW, CH = int(np.ceil(left + right)) + 6, int(up) + 4
    cax, cay = int(np.ceil(left)) + 3, CH - 2                         # ancre (pieds) dans le canevas source
    s = TARGET_H / up
    FW, FH = int(np.ceil(CW * s)) + 2, int(np.ceil(CH * s)) + 1
    fax, fay = int(round(cax * s)) + 1, int(round(cay * s))          # ancre dans l'image finale
    print('source', CW, CH, 'echelle', round(s, 3), 'image', FW, FH, 'ancre', fax, fay)
    SD = []
    for p in P:                                                      # pose placee dans le canevas commun, puis champs de distance
        lay = {}
        for k in ('Sil', 'C', 'E'):
            can = np.zeros((CH, CW), bool); h, w = p[k].shape
            ox, oy = int(round(cax - p['ax'])), int(round(cay - p['ay']))
            can[oy:oy + h, ox:ox + w] = p[k]
            lay[k] = sdf(can)
        SD.append(lay)
    ys, xs = np.mgrid[0:FH, 0:FW]; sy = (ys + 0.5) / s - 0.5; sx = (xs + 0.5) / s - 0.5

    def render(a, b, t):
        lay = {k: (1 - t) * SD[a][k] + t * SD[b][k] for k in ('Sil', 'C', 'E')}
        sil = bil(lay['Sil'], sy, sx) > 0
        c = (bil(lay['C'], sy, sx) > 0) & sil; e = (bil(lay['E'], sy, sx) > 0) & sil
        img = np.zeros((FH, FW), np.uint8); img[sil] = 1; img[c] = 2; img[e] = 3
        pd = np.pad(sil, 1); ring = ~sil & (pd[:-2, 1:-1] | pd[2:, 1:-1] | pd[1:-1, :-2] | pd[1:-1, 2:])
        img[ring] = 4
        return img

    frames = [render(i, i, 0.0) for i in range(6)]
    efirst = []
    for a, b in EDGES:
        efirst.append(len(frames))
        for k in range(1, M + 1): frames.append(render(a, b, k / (M + 1)))
    # fichier d'en-tete
    pix = bytearray()
    for f in frames:
        flat = f.flatten()
        if len(flat) % 2: flat = np.append(flat, 0)
        for i in range(0, len(flat), 2): pix.append(int(flat[i]) << 4 | int(flat[i + 1]))
    rgb565 = lambda c: ((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3)
    o = ['// Genere par tools_cape.py a partir de assets/avatarCape.png -- ne pas editer a la main.', '#pragma once',
         '#define CAPE_W %d' % FW, '#define CAPE_H %d' % FH, '#define CAPE_AX %d' % fax, '#define CAPE_AY %d' % fay,
         '#define CAPE_M %d                       // images intermediaires par arete' % M, '#define CAPE_NE %d' % len(EDGES),
         '#define CAPE_NF %d' % len(frames),
         'static const uint16_t CAPE_PAL[5] = { %s };' % ', '.join('0x%04X' % rgb565(c) for c in PAL),
         'static const uint8_t CAPE_EDGE[CAPE_NE][2] = { %s };   // aretes du graphe de poses (0 repos, 1 marche A, 2 saut, 3 accroupi, 4 marche B, 5 touche)' % ', '.join('{ %d, %d }' % e for e in EDGES),
         'static const uint16_t CAPE_EDGEF[CAPE_NE] = { %s };   // 1re image intermediaire de chaque arete (de e[0] vers e[1])' % ', '.join(str(i) for i in efirst),
         'static const uint8_t CAPE_PIX[%d] = {' % len(pix)]
    for i in range(0, len(pix), 32): o.append('  ' + ','.join(str(v) for v in pix[i:i + 32]) + ',')
    o.append('};')
    open('cape_sprites.h', 'w', encoding='utf-8').write('\n'.join(o) + '\n')
    print('cape_sprites.h :', len(frames), 'images,', len(pix), 'octets')
    # apercu : poses puis intermediaires, fond gris
    cols = 8; rows = (len(frames) + cols - 1) // cols; Z = 2
    sheet = np.full((rows * FH * Z, cols * FW * Z, 3), 120, np.uint8)
    for i, f in enumerate(frames):
        r, c = divmod(i, cols)
        for v in range(1, 5):
            m = np.kron(f == v, np.ones((Z, Z), bool)); blk = sheet[r * FH * Z:(r + 1) * FH * Z, c * FW * Z:(c + 1) * FW * Z]; blk[m] = PAL[v]
    Image.fromarray(sheet).save('assets/_cape_apercu.png')


if __name__ == '__main__':
    main()
