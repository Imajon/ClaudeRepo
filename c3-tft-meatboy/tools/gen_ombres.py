#!/usr/bin/env python3
"""Genere les 30 niveaux du mini-jeu OMBRES -> ombres_levels.h (inclus par le .ino).
Niveaux 1-3 : les niveaux dessines a la main (copies telles quelles). Niveaux 4-30 : labyrinthes generes, difficulte croissante
(lumiere plus petite, plus de gardes, plus de culs-de-sac), gardes qui font des allers-retours en ligne droite (deterministes).
Chaque niveau est VERIFIE : recherche en espace (case, temps) avec les memes regles que le jeu (pas de 11 px / 0,2 s a 55 px/s,
garde vu s'il est dans le rayon de lumiere sans mur entre les deux, ou a moins de 7 px) -> il existe une route sans etre repere,
et elle impose d'attendre ou de contourner (les gardes comptent). Usage : python gen_ombres.py"""
import random
import numpy as np

SWT, MARG, STEP, PL_SPEED = 3, 4, 11, 55.0
DT = 0.2
HORIZON = 300                                   # 60 s
CS = 44                                         # cote d'une case de labyrinthe (4 pas de 11 px)

ORIG = {  # niveaux 1-3 (copie du code d'origine) : murs, depart, sortie, gardes (x, y, dirx, diry), rayon, determinisme
    0: dict(w=[(4, 4, 316, 4), (316, 4, 316, 236), (316, 236, 4, 236), (4, 236, 4, 4), (200, 4, 200, 100), (200, 140, 200, 236), (60, 70, 150, 70), (60, 170, 150, 170)],
            s=(30, 120), e=(292, 218), en=[(260, 30, 0, 1)], R=70, det=1),
    1: dict(w=[(4, 4, 316, 4), (316, 4, 316, 236), (316, 236, 4, 236), (4, 236, 4, 4), (64, 60, 64, 236), (64, 60, 140, 60), (130, 4, 130, 20), (130, 120, 130, 180), (130, 120, 200, 120),
                (200, 50, 200, 120), (236, 180, 316, 180), (260, 50, 260, 130), (200, 180, 200, 236)],
            s=(30, 30), e=(290, 215), en=[(100, 200, 1, 0), (230, 90, 0, 1)], R=76, det=0),
    2: dict(w=[(4, 4, 316, 4), (316, 4, 316, 236), (316, 236, 4, 236), (4, 236, 4, 4), (50, 4, 50, 90), (50, 140, 50, 196), (50, 140, 110, 140), (110, 50, 110, 140), (110, 50, 170, 50),
                (170, 100, 170, 236), (110, 196, 170, 196), (220, 4, 220, 70), (220, 120, 280, 120), (220, 120, 220, 190), (270, 50, 316, 50), (260, 170, 260, 236)],
            s=(25, 220), e=(295, 25), en=[(80, 30, 1, 0), (140, 160, 0, 1), (245, 90, 0, 1)], R=68, det=0),
}


def blocked(walls, x, y, m=MARG):
    if x < m or y < m or x > 320 - m or y > 240 - m: return True
    for (x1, y1, x2, y2) in walls:
        if min(x1, x2) - SWT - m < x < max(x1, x2) + SWT + m and min(y1, y2) - SWT - m < y < max(y1, y2) + SWT + m: return True
    return False


def enemy_track(walls, e):
    """position d'un garde a chaque pas de 0,2 s : aller-retour entre deux obstacles, vitesse 28 * mult"""
    x, y, dx, dy = e
    v = 28.0 * (abs(dx) + abs(dy)); axis = 0 if dx else 1; sg = dx + dy
    p0 = x if axis == 0 else y
    def scan(sign):
        p = p0
        while True:
            q = p + sign
            if (blocked(walls, q, y) if axis == 0 else blocked(walls, x, q)): return p
            p = q
    hi, lo = scan(1), scan(-1)
    L = hi - lo
    out = np.zeros((HORIZON + 1, 2))
    if L <= 0:
        out[:] = (x, y); return out
    P = 2.0 * L
    phase = (p0 - lo) if sg > 0 else L + (hi - p0)
    for k in range(HORIZON + 1):
        s = (phase + v * k * DT) % P
        pos = lo + (s if s <= L else P - s)
        out[k] = (pos, y) if axis == 0 else (x, pos)
    return out


def cross(ax, ay, bx, by, cx, cy):
    return np.sign((bx - ax) * (cy - ay) - (by - ay) * (cx - ax))


def verify(walls, start, exit_, enemies, R, need_wait=True):
    sx, sy = start; ex, ey = exit_
    # treillis de cases accessibles (pas de 11 px a partir du depart)
    xs = np.arange(sx % STEP, 320, STEP); ys = np.arange(sy % STEP, 240, STEP)
    gx, gy = np.meshgrid(xs, ys, indexing='ij')
    free = np.array([[not blocked(walls, int(x), int(y)) for y in ys] for x in xs])
    W = np.array(walls)
    tracks = [enemy_track(walls, e) for e in enemies]
    def unsafe(k):
        u = np.zeros(gx.shape, bool)
        for tr in tracks:
            px, py = tr[k]
            d2 = (gx - px) ** 2 + (gy - py) ** 2
            near = d2 <= (R + 3) ** 2
            close = (np.abs(gx - px) < 11) & (np.abs(gy - py) < 11)
            cand = near & ~close & free
            idx = np.argwhere(cand)
            vis = np.ones(len(idx), bool)
            if len(idx):
                cxp, cyp = gx[idx[:, 0], idx[:, 1]], gy[idx[:, 0], idx[:, 1]]
                for (x1, y1, x2, y2) in W:
                    s1 = cross(cxp, cyp, px, py, x1, y1); s2 = cross(cxp, cyp, px, py, x2, y2)
                    s3 = cross(x1, y1, x2, y2, cxp, cyp); s4 = cross(x1, y1, x2, y2, px, py)
                    vis &= ~((s1 * s2 < 0) & (s3 * s4 < 0))
            u[tuple(idx[vis].T)] = True
            u |= close
        return u
    i0 = int(np.where(xs == sx)[0][0]); j0 = int(np.where(ys == sy)[0][0])
    gi = int(np.argmin(np.abs(xs - ex))); gj = int(np.argmin(np.abs(ys - ey)))
    if abs(xs[gi] - ex) >= 9 or abs(ys[gj] - ey) >= 9: return None
    def shift(a):
        b = a.copy(); b[1:, :] |= a[:-1, :]; b[:-1, :] |= a[1:, :]; b[:, 1:] |= a[:, :-1]; b[:, :-1] |= a[:, 1:]; return b
    # route libre (sans gardes) : temps minimal
    cur = np.zeros(gx.shape, bool); cur[i0, j0] = True; t_free = None
    for k in range(1, HORIZON + 1):
        cur = shift(cur) & free
        if cur[gi, gj]: t_free = k; break
    if t_free is None: return None
    cur = np.zeros(gx.shape, bool); cur[i0, j0] = True
    if unsafe(0)[i0, j0]: return None
    for k in range(1, HORIZON + 1):
        cur = shift(cur) & free & ~unsafe(k)
        if cur[gi, gj]: return k, t_free
        if not cur.any(): return None
    return None


def maze(rng, cols, rows, loops):
    openings = set()
    seen = {(0, 0)}; st = [(0, 0)]
    while st:
        i, j = st[-1]
        nb = [(i + a, j + b) for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1)) if 0 <= i + a < cols and 0 <= j + b < rows and (i + a, j + b) not in seen]
        if not nb: st.pop(); continue
        n = rng.choice(nb); openings.add(frozenset(((i, j), n))); seen.add(n); st.append(n)
    edges = [frozenset(((i, j), (i + 1, j))) for i in range(cols - 1) for j in range(rows)] + [frozenset(((i, j), (i, j + 1))) for i in range(cols) for j in range(rows - 1)]
    closed = [e for e in edges if e not in openings]
    rng.shuffle(closed)
    for e in closed[:int(len(closed) * loops)]: openings.add(e)
    return openings


def walls_from(openings, cols, rows, ox, oy):
    segs = []
    W, H = cols * CS, rows * CS
    segs += [(ox, oy, ox + W, oy), (ox + W, oy, ox + W, oy + H), (ox + W, oy + H, ox, oy + H), (ox, oy + H, ox, oy)]
    for i in range(1, cols):                                       # lignes verticales : on fusionne les cases voisines fermees
        j = 0
        while j < rows:
            if frozenset(((i - 1, j), (i, j))) not in openings:
                j2 = j
                while j2 + 1 < rows and frozenset(((i - 1, j2 + 1), (i, j2 + 1))) not in openings: j2 += 1
                segs.append((ox + i * CS, oy + j * CS, ox + i * CS, oy + (j2 + 1) * CS)); j = j2 + 1
            else: j += 1
    for j in range(1, rows):
        i = 0
        while i < cols:
            if frozenset(((i, j - 1), (i, j))) not in openings:
                i2 = i
                while i2 + 1 < cols and frozenset(((i2 + 1, j - 1), (i2 + 1, j))) not in openings: i2 += 1
                segs.append((ox + i * CS, oy + j * CS, ox + (i2 + 1) * CS, oy + j * CS)); i = i2 + 1
            else: i += 1
    return segs


def make(n):
    d = (n - 3) / 26.0                                             # difficulte 0..1 (n = indice 3..29)
    cols = 3 + int(round(4 * d)); rows = 3 + int(round(2 * d))
    ox = 6 + (7 - cols) * CS // 2; oy = 10 + (5 - rows) * CS // 2
    R = int(round(78 - 24 * d)); ne = 1 + int(d * 5 + 0.5)
    for seed in range(400):
        rng = random.Random(n * 7919 + seed)
        op = maze(rng, cols, rows, 0.30 - 0.18 * d)
        walls = walls_from(op, cols, rows, ox, oy)
        def center(c): return ox + CS // 2 + c[0] * CS, oy + CS // 2 + c[1] * CS
        s = (0, rows - 1); e = (cols - 1, 0)
        enemies = []
        used = {s, e}
        for k in range(ne):
            c = (rng.randrange(cols), rng.randrange(rows))
            if c in used or (abs(c[0] - s[0]) + abs(c[1] - s[1])) < 2: continue
            used.add(c); x, y = center(c)
            horiz = rng.random() < 0.5
            mult = 2 if (d > 0.55 and k == 0 and rng.random() < 0.5) else 1
            dirv = rng.choice([-1, 1]) * mult
            enemies.append((x, y, dirv, 0) if horiz else (x, y, 0, dirv))
        if not enemies: continue
        r = verify(walls, center(s), center(e), enemies, R)
        if r is None: continue
        t_with, t_free = r
        if n >= 6 and t_with < t_free + 8: continue                    # les gardes doivent compter (au moins 1,6 s d'attente ou de detour)
        return dict(w=walls, s=center(s), e=center(e), en=enemies, R=R, det=1), (t_with * DT, t_free * DT, seed)
    raise SystemExit('niveau %d : aucune variante valide' % (n + 1))


def main():
    out = ['// Genere par gen_ombres.py -- ne pas editer a la main. 30 niveaux : 1-3 dessines a la main, 4-30 labyrinthes verifies.', '#pragma once', '#define SH_N 30']
    levels = []
    for n in range(30):
        if n in ORIG:
            L = ORIG[n]
            r = verify(L['w'], L['s'], L['e'], L['en'], L['R']) if L['det'] else None
            print('niveau %2d (dessine)  %s' % (n + 1, 'verifie' if r else 'aleatoire/non verifiable'))
        else:
            L, info = make(n)
            print('niveau %2d  %2d murs %d gardes R=%d : route %.1f s (libre %.1f s) graine %d' % (n + 1, len(L['w']), len(L['en']), L['R'], info[0], info[1], info[2]))
        levels.append(L)
    for n, L in enumerate(levels):
        out.append('static const SWall SW%d[] = { ' % (n + 1) + ', '.join('{ %d, %d, %d, %d }' % w for w in L['w']) + ' };')
        out.append('static const int16_t SE%d[] = { ' % (n + 1) + ', '.join('%d, %d, %d, %d' % e for e in L['en']) + ' };')
    out.append('static const SLvl SLV[SH_N] = {')
    for n, L in enumerate(levels):
        out.append('  { SW%d, sizeof(SW%d) / sizeof(SWall), %d, %d, %d, %d, SE%d, %d, %d, %d },' % (n + 1, n + 1, L['s'][0], L['s'][1], L['e'][0], L['e'][1], n + 1, len(L['en']), L['R'], L['det']))
    out.append('};')
    open('ombres_levels.h', 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    print('ombres_levels.h ecrit')


if __name__ == '__main__':
    main()
