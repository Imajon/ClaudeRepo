#!/usr/bin/env python3
"""Mini-jeu SAMOURAI (facon Prince of Persia) :
1. sprite du samourai a partir de la sequence JPG (assets/samourai/<dossier>/*.jpg : planche IDLES / RUNS / ATTACKS / DEATHS
   filmee en video, 2 variantes par rangee ; on prend celle de gauche) -> sam_sprites.h (meme format PAnim 4 bits que perso_sprites.h) ;
2. niveaux de samourai_levels.txt -> samourai_levels.h, avec verification qu'on peut aller du depart a la sortie
   (marcher, sauter un vide de 2, grimper un rebord jusqu'a 4 tuiles, tomber ; grilles supposees ouvertes, caisses ignorees) ;
3. apercus : assets/_samourai_sprite.png et assets/_samourai_niveaux.png (plans des niveaux).
Usage : python tools_samourai.py"""
import glob, collections
import numpy as np
from PIL import Image, ImageDraw
from tools_perso import kmeans

BG = np.array([92, 92, 92])                       # gris du fond de la planche
# (nom, boite de la cellule (x0, y0, x1, y1) dans l'image 300x300, images video [debut, fin], duree d'une image ms)
ANIMS = [('SAM_IDLE', (120, 0, 190, 36), (0, 24), 100), ('SAM_RUN', (120, 70, 190, 106), (0, 16), 70),
         ('SAM_ATK', (90, 146, 200, 182), (36, 49), 50), ('SAM_DIE', (110, 230, 200, 268), (32, 49), 80)]


def components(m):                                 # garde le personnage (plus grand amas, 8-connexe) : sans le reflet ni les eclats isoles
    lab = np.zeros(m.shape, int); n = 0; sizes = {}
    for y, x in zip(*np.where(m)):
        if lab[y, x]: continue
        n += 1; st = [(y, x)]; lab[y, x] = n; k = 0
        while st:
            cy, cx = st.pop(); k += 1
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = cy + dy, cx + dx
                    if 0 <= yy < m.shape[0] and 0 <= xx < m.shape[1] and m[yy, xx] and not lab[yy, xx]: lab[yy, xx] = n; st.append((yy, xx))
        sizes[n] = k
    if not sizes: return m
    big = max(sizes, key=sizes.get)
    return lab == big


def sprites():
    fs = sorted(glob.glob('assets/samourai/*/*.jpg'))
    A = np.array([np.array(Image.open(f).convert('RGB')).astype(int) for f in fs])
    out = []
    for name, (x0, y0, x1, y1), (f0, f1), ms in ANIMS:
        cell = A[:, y0:y1, x0:x1]
        m0 = np.abs(cell[f0] - BG).sum(2) > 60
        rows = np.where(m0.any(1))[0]; cut = rows[0]
        while cut + 1 < m0.shape[0] and m0[cut + 1].any(): cut += 1   # bas du personnage = premiere rangee vide (le reflet est dessous)
        seq = []
        for f in range(f0, f1 + 1):
            img = cell[f, :cut + 1]
            m = components(np.abs(img - BG).sum(2) > 60)
            if seq and np.abs(img - seq[-1][0]).mean() < 0.8 and (m == seq[-1][1]).mean() > 0.99: continue   # image repetee par la video
            seq.append((img, m))
        out.append((name, seq, ms))
    # cadre commun : union des masques, ancre = pieds de la premiere image d'idle
    pal = kmeans(np.concatenate([i[m] for _, seq, _ in out for i, m in seq]), 15)
    L = ['// Genere par tools_samourai.py a partir de assets/samourai/*.jpg -- ne pas editer a la main.', '#pragma once',
         '#include "perso_sprites.h"   // struct PAnim', 'const uint16_t SAM_PAL[16] = { 0, ' +
         ', '.join('0x%04X' % (((int(c[0]) >> 3) << 11) | ((int(c[1]) >> 2) << 5) | (int(c[2]) >> 3)) for c in pal) + ' };']
    pix = []; table = []; prev = []
    for name, seq, ms in out:
        U = np.any([m for _, m in seq], 0); ys, xs = np.where(U)
        cy0, cy1, cx0, cx1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        ib, mb = seq[0][1][-4:], None
        fx = np.where(seq[0][1][-3:].any(0))[0]
        ax, ay = int(np.median(fx)) - cx0, cy1 - cy0
        table.append((name, len(seq), cx1 - cx0, cy1 - cy0, ax, ay, len(pix), ms))
        for img, m in seq:
            r, o = img[cy0:cy1, cx0:cx1], m[cy0:cy1, cx0:cx1]
            d = ((r[:, :, None, :] - pal[None, None]) ** 2).sum(3).argmin(2)
            prev.append((name, np.where(o[:, :, None], pal[d], [[60, 66, 80]]).astype(np.uint8)))
            d = d + 1; d[~o] = 0; flat = d.flatten().tolist()
            if len(flat) % 2: flat.append(0)
            pix += [(flat[i] << 4) | flat[i + 1] for i in range(0, len(flat), 2)]
        print(name, len(seq), 'images', cx1 - cx0, 'x', cy1 - cy0, 'ancre', ax, ay)
    L.append('const uint8_t SAM_PIX[%d] = {' % len(pix))
    for i in range(0, len(pix), 32): L.append('  ' + ','.join(str(v) for v in pix[i:i + 32]) + ',')
    L.append('};')
    for i, t in enumerate(table): L.append('#define %s %d' % (t[0], i))
    L.append('const PAnim SAM_ANIM[%d] = {' % len(table))
    for t in table: L.append('  { 0, %d, %d, %d, %d, %d, 1, %du },   // %s' % (t[1], t[2], t[3], t[4], t[5], t[6], t[0]))
    L.append('};')
    L.append('const uint16_t SAM_MS[%d] = { %s };' % (len(table), ', '.join(str(t[7]) for t in table)))
    open('sam_sprites.h', 'w', encoding='utf-8').write('\n'.join(L) + '\n')
    W = sum(p.shape[1] * 3 + 6 for _, p in prev[:40]); sheet = Image.new('RGB', (min(W, 3000), 4 * 130), (40, 40, 50))
    x = {n: 0 for n, *_ in out}; row = {n: i for i, (n, *_) in enumerate(out)}
    for n, p in prev:
        if x[n] + p.shape[1] * 3 < 3000: sheet.paste(Image.fromarray(p).resize((p.shape[1] * 3, p.shape[0] * 3), Image.NEAREST), (x[n], row[n] * 130 + 4)); x[n] += p.shape[1] * 3 + 6
    sheet.save('assets/_samourai_sprite.png')
    print('sam_sprites.h', len(pix), 'octets')


def load_levels():
    lv = []; cur = None
    import os
    txt = open('samourai_levels.txt', encoding='utf-8').read()
    if os.path.exists('samourai_levels_gen.txt'): txt += '\n' + open('samourai_levels_gen.txt', encoding='utf-8').read()   # niveaux 7-30 produits par gen_samourai.py
    for l in txt.split('\n'):
        l = l.rstrip()
        if l.startswith('#') and cur is None: continue
        if l.startswith('= '):
            t, tip, th = (l[2:].split(' | ') + ['', ''])[:3]; cur = [t.strip(), tip.strip(), [], int(th or 0)]; lv.append(cur)
        elif l and cur is not None: cur[2].append(l)
    for t, tip, g, th in lv:
        assert len(set(map(len, g))) == 1, t + ' : rangees de largeurs differentes'
        assert len(g[0]) <= 64 and len(g) <= 32, t + ' : trop grand (64 x 32 max)'
        assert sum(r.count('@') for r in g) == 1 and sum(r.count('E') for r in g) == 1, t + ' : il faut un @ et un E'
    return lv


def reachable(g):
    """BFS sur les positions debout (colonne, rangee du sol). Grilles ouvertes, caisses ignorees, dalles fragiles = sol."""
    H, W = len(g), len(g[0])
    sol = lambda c, r: 0 <= r < H and 0 <= c < W and g[r][c] in '#x'
    free = lambda c, r: 0 <= r < H and 0 <= c < W and g[r][c] not in '#x'
    def stand(c, r): return sol(c, r) and free(c, r - 1) and free(c, r - 2)
    def land(c, r):                                    # tombe dans la colonne c depuis la rangee r (corps), renvoie le sol
        while r < H and not sol(c, r): r += 1
        return r if r < H and stand(c, r) else None
    for r in range(H):
        for c in range(W):
            if g[r][c] == '@': start = (c, r + 1)
            if g[r][c] == 'E': goal = (c, r + 1)
    seen = {start}; q = collections.deque([start]); hurt = set()
    while q:
        c, r = q.popleft(); nxt = []
        for d in (-1, 1):
            fc = c + d
            if free(fc, r - 1) and free(fc, r - 2):    # marcher / tomber du bord
                if stand(fc, r): nxt.append((fc, r))
                else:
                    lr = land(fc, r)
                    if lr is not None: nxt.append((fc, lr)); (hurt.add((fc, lr)) if lr - r > 5 else None)
                for gap in (1, 2):                      # sauter un vide (meme niveau ou plus bas)
                    tc = c + d * (gap + 1)
                    if all(free(c + d * k, r - 1) and free(c + d * k, r - 2) and free(c + d * k, r - 3) for k in range(1, gap + 1)):
                        lr = land(tc, r - 1) if free(tc, r - 1) else None
                        if lr is not None and lr >= r: nxt.append((tc, lr))
            for h in range(1, 5):                       # grimper un rebord
                if stand(fc, r - h) and all(free(c, rr) for rr in range(r - h - 2, r)): nxt.append((fc, r - h)); break
        for n in nxt:
            if n not in seen: seen.add(n); q.append(n)
    return goal in seen, seen


def levels(lv):
    L = ['// Genere par tools_samourai.py a partir de samourai_levels.txt -- ne pas editer a la main.', '#pragma once',
         '#define SAM_NLV %d' % len(lv), 'struct SamLv { const char *title, *tip; uint8_t w, h, theme; const char *const *rows; };']
    for i, (t, tip, g, th) in enumerate(lv):
        L.append('static const char *const SAM_LV%d[%d] = {' % (i, len(g)))
        for r in g: L.append('  "%s",' % r)
        L.append('};')
    L.append('const SamLv SAM_LVS[SAM_NLV] = {')
    for i, (t, tip, g, th) in enumerate(lv): L.append('  { "%s", "%s", %d, %d, %d, SAM_LV%d },' % (t, tip.replace('"', "'"), len(g[0]), len(g), th, i))
    L.append('};')
    open('samourai_levels.h', 'w', encoding='utf-8').write('\n'.join(L) + '\n')
    ok = True; imgs = []
    COL = {'#': (92, 88, 104), 'x': (150, 120, 90), '^': (220, 60, 60), 'K': (170, 110, 50), 'E': (240, 200, 80), '@': (90, 220, 120),
           'G': (170, 100, 220), '*': (255, 240, 120), '.': (24, 22, 34)}
    for i, (t, tip, g, th) in enumerate(lv):
        good, seen = reachable(g)
        ok &= good
        print('niveau %d %-24s %dx%d  %s' % (i + 1, t, len(g[0]), len(g), 'OK' if good else 'BLOQUE'))
        s = 12; im = Image.new('RGB', (len(g[0]) * s, len(g) * s + 30), (16, 14, 22)); d = ImageDraw.Draw(im)
        d.text((4, 4), 'Niveau %d : %s' % (i + 1, t), fill=(255, 230, 160)); d.text((4, 16), tip, fill=(180, 180, 200))
        for r, row in enumerate(g):
            for c, ch in enumerate(row):
                x, y = c * s, r * s + 30
                if ch in 'ABC': d.rectangle([x + 3, y, x + s - 4, y + s - 1], fill=(140, 150, 170))
                elif ch in 'abc': d.rectangle([x + 2, y + s - 4, x + s - 3, y + s - 1], fill=(120, 90, 60)); d.line([x + s // 2, y + s - 4, x + s - 2, y + 2], fill=(230, 200, 90), width=2)
                elif ch in '123': d.rectangle([x + 1, y + s - 3, x + s - 2, y + s - 1], fill=(90, 200, 230))
                elif ch == 'v': d.polygon([(x, y), (x + s // 2, y + s - 3), (x + s, y)], fill=COL['^'])
                elif ch in 'OSM': d.ellipse([x, y, x + s - 1, y + s - 1], fill=(220, 60, 80) if ch == 'M' else (110, 110, 120)); d.text((x + 3, y), ch, fill=(255, 255, 255))
                elif ch == 'V': d.rectangle([x + 2, 30, x + s - 3, y + s - 1], fill=(220, 60, 80))
                elif ch == '^': d.polygon([(x, y + s), (x + s // 2, y + 3), (x + s, y + s)], fill=COL['^'])
                else: d.rectangle([x, y, x + s - 1, y + s - 1], fill=COL.get(ch, (24, 22, 34)))
                if ch in 'ABC123abc': d.text((x + 2, y), ch, fill=(255, 255, 255))
        for c, r in seen: d.rectangle([c * s + 4, (r - 1) * s + 30 + 5, c * s + 7, (r - 1) * s + 30 + 8], fill=(80, 160, 90))   # endroits atteints
        imgs.append(im)
    Wd = max(im.width for im in imgs); sheet = Image.new('RGB', (Wd, sum(im.height + 8 for im in imgs)), (8, 8, 12)); y = 0
    for im in imgs: sheet.paste(im, (0, y)); y += im.height + 8
    sheet.save('assets/_samourai_niveaux.png')
    if not ok: raise SystemExit('un niveau est bloque (voir assets/_samourai_niveaux.png)')


if __name__ == '__main__':
    sprites()
    levels(load_levels())
