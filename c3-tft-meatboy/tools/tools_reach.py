#!/usr/bin/env python3
"""Verifie hors carte que les 50 mondes (dump.txt, produit par la compilation -DLEVEL_DUMP) sont franchissables :
graphe des surfaces (dessus de blocs) relie par la marche et par des sauts dont la portee vient de la physique du jeu.
Usage : python tools_reach.py [dump.txt]"""
import sys, math, collections

RUN, JUMP_V, G, TILE = 200.0, -450.0, 1400.0, 16


def reach_table(speed):
    """portee horizontale max (px) d'un saut maintenu pour une denivelee dh (tuiles, + = cible plus haute), depart bord a bord, avec elan"""
    v = RUN * speed
    tab = {}
    for dh in range(-12, 6):
        target = dh * TILE                        # hauteur des pieds de la cible au-dessus du depart
        t, y, vy, x, best = 0.0, 0.0, JUMP_V, 0.0, None
        dt = 0.002
        while t < 3.0:
            vy += (G * 1.12 if vy > 0 else G) * dt
            y -= vy * dt; x = v * t; t += dt           # y = hauteur (+ vers le haut)
            if vy > 0 and y <= target and t > 0.05:
                best = x; break
        tab[dh] = best
    return tab


def load(path):
    lv = {}; cur = None
    for l in open(path, encoding='utf-8', errors='ignore').read().split('\n'):
        l = l.rstrip('\r')
        if l.startswith('LV '):
            _, i, th, sp = l.split(); cur = {'idx': int(i), 'theme': int(th), 'spd': float(sp), 'rows': [], 'vines': []}; lv[cur['idx']] = cur
        elif l == 'END': cur = None
        elif l.startswith('VN ') and cur: cur['vines'].append(tuple(map(int, l.split()[1:])))
        elif cur is not None and l and not l.startswith('DONE') and set(l) <= set('.SXTkcmxvhLPAGO*gbuFpWfreosNzZyUQ'): cur['rows'].append(l)
    return lv


def analyse(lv):
    rows = lv['rows']; H = len(rows); W = len(rows[0])
    solid = lambda r, c: 0 <= r < H and 0 <= c < W and rows[r][c] in 'SXzZ'
    tab = reach_table(lv['spd'])
    surf = []
    for c in range(W):
        for r in range(H):
            if solid(r, c) and not solid(r - 1, c) and r > 0:
                surf.append((c, r))
    sset = set(surf)
    start = next((n for n in surf if n[0] >= 2 and rows[n[1] - 1][n[0]] in '.PgW'), None)
    goal = None
    for r in range(H):
        for c in range(W):
            if rows[r][c] == 'G': goal = (c, r)
    # sol de depart : colonne 2, premiere surface
    start = next(n for n in surf if n[0] == 2)
    # lianes : relient les bords d'un vide (franchissable en simulation, cf. vinesim.py)
    vine_cols = sorted((v[0] - 8) // TILE for v in lv['vines'])
    adj = collections.defaultdict(list)
    for (c, r) in surf:
        for (c2, r2) in surf:
            if c2 == c: continue
            gap = abs(c2 - c) - 1
            dh = r - r2                              # + = cible plus haute
            if dh > 4:                                   # saut de mur en mur : couloir de 1 a 3 tuiles entre la colonne cible et une paroi d'en face
                ok = abs(c2 - c) <= 6 and all(solid(rr, c2) for rr in range(r2, r)) and any(
                    sum(solid(rr, w) for rr in range(r2, r)) >= 3 for w in (c2 - 2, c2 - 3, c2 - 4, c2 + 2, c2 + 3, c2 + 4))
                if ok: adj[(c, r)].append((c2, r2))
                continue
            reach = tab.get(max(-12, min(5, dh)))
            if reach is None: continue
            # portee disponible : le joueur part du bord (largeur 12 px) -> il peut combler `gap` tuiles vides + empieter de 4 px
            if gap * TILE + 4 > reach - 6: continue
            lo, hi = sorted((c, c2))
            ok = True
            for cc in range(lo + 1, hi):                     # obstacle entre les deux : colonne trop haute pour l'arc
                tops = [rr for rr in range(H) if solid(rr, cc)]
                if tops and (r - min(tops)) > 3.2 and (r2 - min(tops)) > 3.2: ok = False; break
            if ok: adj[(c, r)].append((c2, r2))
    # lianes
    if vine_cols:
        for (c, r) in surf:
            for (c2, r2) in surf:
                if c2 > c and r2 == r and c2 - c > 10 and any(c < vc < c2 for vc in vine_cols):
                    adj[(c, r)].append((c2, r2)); adj[(c2, r2)].append((c, r))
    seen = {start}; q = collections.deque([start])
    while q:
        n = q.popleft()
        for m in adj[n]:
            if m not in seen: seen.add(m); q.append(m)
    # le drapeau est pose sur le plateau : on cherche une surface atteinte a moins de 2 colonnes
    reached = goal is not None and any(abs(c - goal[0]) <= 2 and abs(r - goal[1]) <= 6 for (c, r) in seen)
    maxcol = max((c for (c, r) in seen), default=0)
    return reached, maxcol, len(surf)


if __name__ == '__main__':
    lvs = load(sys.argv[1] if len(sys.argv) > 1 else 'dump.txt')
    bad = 0
    for i in sorted(lvs):
        if len(set(len(x) for x in lvs[i]['rows'])) != 1: print(f'niveau {i + 1:2d} : donnees serie corrompues (a relancer)'); continue
        ok, mc, n = analyse(lvs[i])
        if not ok: bad += 1
        print(f"niveau {i + 1:2d} theme {lvs[i]['theme']} vitesse {lvs[i]['spd']}  {'OK' if ok else 'BLOQUE a la colonne ' + str(mc)}")
    print('bloques :', bad, '/', len(lvs))
