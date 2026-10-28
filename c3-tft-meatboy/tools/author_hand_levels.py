#!/usr/bin/env python3
"""Outil PONCTUEL (pas appele par le build) : genere un PREMIER JET de
hand_levels.txt pour les niveaux dessines a la main (les 3 premiers de
chaque dizaine : 0,1,2,10,11,12,...,90,91,92), avec une difficulte qui
augmente par dizaine (gaps, hauteurs, densite de pieges).

Objectif : donner un point de depart correct et jouable (gaps/hauteurs
dans les memes bornes que le generateur procedural existant) que
l'utilisateur redessinera/ajustera a la main dans hand_levels.txt.

Usage : python author_hand_levels.py > hand_levels.txt
"""
import sys

COLS = 20
ROWS = 15   # meme hauteur que les niveaux "corridor" du jeu (15*16=240=SCREEN_H)
BASE_ROW = 11  # meme ligne de base que buildChain() dans le .ino


class Grid:
    def __init__(self):
        self.g = [['.'] * COLS for _ in range(ROWS)]
        self.anchors = []  # (col, row) centre de chaque plateforme, pour poser les etoiles

    def putH(self, c0, c1, r, ch):
        if r < 0 or r >= ROWS:
            return
        for c in range(max(0, c0), min(COLS, c1 + 1)):
            self.g[r][c] = ch

    def star_above(self, col, row):
        for ty in (row - 2, row - 1):
            if 0 <= ty < ROWS and self.g[ty][col] == '.':
                self.g[ty][col] = '*'
                return

    def dump(self):
        return ["".join(r) for r in self.g]


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def build_level(platforms):
    """platforms: liste de dicts {w, dr, hazard} ; le 1er est la plateforme de
    depart (toujours large, sans piege), gap fixe avant chaque suivante."""
    grid = Grid()
    row = BASE_ROW
    col = 0
    first = platforms[0]
    grid.putH(col, col + first['w'] - 1, row, '#')
    grid.putH(col, col + first['w'] - 1, row + 1, '#')  # double epaisseur sous le depart
    grid.g[row - 1][col + 1] = 'S'
    grid.anchors.append((col + first['w'] // 2, row))
    col += first['w']

    last_c0 = 0
    for i, p in enumerate(platforms[1:], 1):
        gap = p['gap']
        row = clamp(row + p['dr'], 4, 12)
        c0 = col + gap
        c1 = c0 + p['w'] - 1
        if c1 > COLS - 1:
            c1 = COLS - 1
            if c1 - c0 < 1:
                break
        tile = 'C' if p['hazard'] == 'crumble' else '#'
        grid.putH(c0, c1, row, tile)
        if p['hazard'] == 'spike':
            mid = (c0 + c1) // 2
            grid.g[row - 1][mid] = '^'
        grid.anchors.append(((c0 + c1) // 2, row))
        col = c1 + 1
        last_c0, last_c1, last_row = c0, c1, row

    # drapeau au-dessus du centre de la derniere plateforme
    mid = (last_c0 + last_c1) // 2
    grid.g[last_row - 1][mid] = 'G'

    # etoiles : sur 3 plateformes intermediaires bien reparties (pas la 1ere ni la derniere)
    mids = grid.anchors[1:-1] if len(grid.anchors) > 2 else grid.anchors
    if mids:
        idxs = sorted(set([0, len(mids) // 2, len(mids) - 1]))
        for i in idxs[:3]:
            c, r = mids[i]
            grid.star_above(c, r)

    return grid.dump()


def platforms_for(tier, variant):
    """tier 0..9 (dizaine), variant 0/1/2 (les 3 niveaux dessines de la dizaine).
    Monte en difficulte avec le tier : gaps, largeurs, pieges, denivele."""
    gap_max = min(4, 2 + tier // 3)
    width_min = max(2, 5 - tier // 2)
    dr_max = min(3, 1 + tier // 3)
    # nb de plateformes a pieges + type selon le tier
    if tier <= 1:
        hazard_kinds = []
    elif tier <= 4:
        hazard_kinds = ['spike']
    else:
        hazard_kinds = ['spike', 'crumble']

    n_platforms = 4 + (variant % 3)          # 4..6 plateformes apres le depart
    platforms = [{'w': 4}]                   # plateforme de depart, large, jamais de piege
    for i in range(n_platforms):
        gap = 2 + ((i + variant) % (gap_max - 1)) if gap_max > 2 else 2
        w = width_min + ((i + variant) % 2)
        dr_sign = 1 if (i + variant) % 2 == 0 else -1
        dr = dr_sign * (1 if tier < 4 else (1 + (i % dr_max)))
        hazard = None
        if hazard_kinds and (i + variant) % 2 == 1 and i > 0:
            hazard = hazard_kinds[(i + variant) % len(hazard_kinds)]
        platforms.append({'gap': gap, 'w': w, 'dr': dr, 'hazard': hazard})
    return platforms


def main():
    out = sys.stdout
    out.write("# Niveaux dessines a la main (3 premiers de chaque dizaine : 0,1,2,10,11,12,...,90,91,92).\n")
    out.write("# Le reste (3-9 de chaque dizaine) reste procedural (genLevel() dans le .ino).\n")
    out.write("# Format : 'LEVEL <n>' puis des rangees de 20 caracteres exactement, puis 'END'.\n")
    out.write("# Tuiles : # bloc | C plateforme fragile | ^ pic fixe | S depart | G drapeau | * etoile | . vide\n")
    out.write("# Premier jet genere par author_hand_levels.py (gaps/hauteurs dans les memes bornes\n")
    out.write("# que le generateur procedural) -- a redessiner/ajuster a la main ensuite.\n")
    out.write("# Relu automatiquement par gen_hand_levels.py a chaque compilation.\n\n")
    for tier in range(10):
        for variant in range(3):
            n = tier * 10 + variant
            platforms = platforms_for(tier, variant)
            rows = build_level(platforms)
            out.write(f"LEVEL {n}\n")
            for r in rows:
                out.write(r + "\n")
            out.write("END\n\n")


if __name__ == "__main__":
    main()
