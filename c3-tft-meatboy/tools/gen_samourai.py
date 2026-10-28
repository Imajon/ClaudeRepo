#!/usr/bin/env python3
"""Genere les niveaux 7 a 30 du mini-jeu SAMOURAI (difficulte croissante) et les ajoute a samourai_levels.txt apres les 6 niveaux dessines a la main
(ils sont gardes tels quels : tout ce qui suit la ligne MARQUEUR est regenere). Chaque niveau est verifie par tools_samourai.reachable().
Usage : python gen_samourai.py   puis   python -c "import tools_samourai as t; t.levels(t.load_levels())" (fait par ce script)"""
import random
import tools_samourai as T

TITLES = ['Les remparts', 'Le pont brise', 'Escalier des ombres', 'La fosse aux pointes', 'Corridor des gardes', 'Marches de pierre', 'Le gouffre', 'La cour du temple',
          'Passage etroit', 'Les deux gardes', 'La longue descente', 'Montee interrompue', 'Le tunnel scelle', 'Cimes et abimes', 'Chemin des fantomes', 'La grande salle',
          'Pierres et pointes', 'Le dernier rempart', 'Veillee', 'Les trois fosses', 'Escalade nocturne', 'La porte du levier', 'Le labyrinthe vertical', 'Le sanctuaire']
TIPS = ['Saute les vides de 2 cases', 'SAUT face a un rebord : on grimpe', 'Les pointes coutent un coeur : saute par-dessus', 'Un fantome a portee : un coup de SAUT',
        'Ne tombe pas de plus de 5 cases', 'Le levier ouvre la grille du tunnel']


def build(i, rng):
    d = i / 23.0                                         # difficulte 0..1
    W = 44 + int(20 * d) + rng.randint(-2, 2); W = max(40, min(64, W))
    H = 16 + int(6 * d)
    B = H - 3
    g = [['.'] * W for _ in range(H)]
    top = [B] * W
    feats = set()
    def solid_from(c, t):
        for r in range(t, H): g[r][c] = '#'
    c = 1
    for k in range(1, 5): top[k] = B
    g[B - 1][2] = '@'
    c = 5
    last_gate = False
    while c < W - 9:
        kind = rng.choices(['run', 'up', 'down', 'pit', 'spike', 'ghost', 'box', 'tunnel', 'star'],
                           [3, 2 + d, 2 + d, 1.5 + 2 * d, 1 + 2 * d, 0.5 + 2.5 * d, 1, 0.3 + 1.2 * d if not last_gate else 0, 1])[0]
        cur = top[c - 1]
        if kind == 'run':
            n = rng.randint(3, 6)
            for k in range(n): top[c + k] = cur
            c += n
        elif kind == 'up' and cur - 1 >= 6:
            h = rng.randint(1, min(3, cur - 6)); top[c] = cur - h; n = rng.randint(2, 4)
            for k in range(n): top[c + k] = cur - h
            c += n; feats.add('up')
        elif kind == 'down' and cur + 1 <= B:
            h = rng.randint(1, min(3, B - cur)); n = rng.randint(2, 4)
            for k in range(n): top[c + k] = cur + h
            c += n; feats.add('down')
        elif kind == 'pit':
            gap = rng.choice([1, 2]); depth = min(H - 2, cur + 5)
            for k in range(gap): top[c + k] = depth
            for k in range(3): top[c + gap + k] = cur
            for k in range(gap): g[depth - 1][c + k] = '^'
            c += gap + 3; feats.add('pit')
        elif kind == 'spike':
            n = rng.randint(5, 7)
            for k in range(n): top[c + k] = cur
            g[cur - 1][c + 2 + rng.randint(0, 1)] = '^'
            c += n; feats.add('spike')
        elif kind == 'ghost':
            n = rng.randint(6, 8)
            for k in range(n): top[c + k] = cur
            g[cur - 1][c + n // 2] = 'G'
            c += n; feats.add('ghost')
        elif kind == 'box':
            n = rng.randint(5, 7)
            for k in range(n): top[c + k] = cur
            g[cur - 1][c + 1] = 'K'
            c += n
        elif kind == 'tunnel' and cur >= 7:
            n = 9
            for k in range(n): top[c + k] = cur
            for k in range(n):                                       # plafond plein au-dessus (4 cases libres)
                for r in range(1, cur - 4): g[r][c + k] = '#'
            g[cur - 1][c + 1] = 'a'
            for r in range(cur - 4, cur): g[r][c + 5] = 'A'
            c += n; last_gate = True; feats.add('gate')
        elif kind == 'star':
            n = rng.randint(3, 5)
            for k in range(n): top[c + k] = cur
            g[cur - 3][c + 1] = '*'
            c += n
        else:
            for k in range(3): top[c + k] = cur
            c += 3
    cur = top[c - 1]
    for k in range(c, W - 1): top[k] = cur
    g[top[W - 4] - 1][W - 4] = 'E'
    for x in range(W): solid_from(x, top[x])
    for r in range(H): g[r][0] = '#'; g[r][W - 1] = '#'
    for x in range(W): g[0][x] = '#'
    # les cases A (grille) restent libres de pierre ; le plafond du tunnel a ete pose avant : on le retire sous la grille
    return [''.join(r) for r in g], feats


def make(i):
    for seed in range(200):
        rng = random.Random(i * 1000 + seed)
        g, feats = build(i, rng)
        good, _ = T.reachable(g)
        if good and ('pit' in feats or 'spike' in feats or 'ghost' in feats or 'gate' in feats) and sum(r.count('G') for r in g) <= 2 + i // 6:
            return g, feats
    raise SystemExit('niveau %d : aucune variante valide' % (i + 7))


def main():
    out = []
    for i in range(23):
        g, feats = make(i)
        tip = TIPS[0] if 'pit' in feats else TIPS[2] if 'spike' in feats else TIPS[3] if 'ghost' in feats else TIPS[5] if 'gate' in feats else TIPS[1]
        out.append('= %s | %s | %d\n%s\n' % (TITLES[i], tip, 1 if i % 5 == 4 else 0, '\n'.join(g)))
    open('samourai_levels_gen.txt', 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    T.levels(T.load_levels())


if __name__ == '__main__':
    main()
