#!/usr/bin/env python3
"""Outil PONCTUEL : reecrit les niveaux 0-9 dans hand_levels.txt avec 10
layouts DISTINCTS dessines a la main (pas de gabarit repete), pour repondre
au retour "aucun niveau qui se repete, plus de plateformes / plus de
variation de hauteur" sur la dizaine 1. Garde intacts les niveaux 10+ deja
presents dans hand_levels.txt (dizaines suivantes, a retravailler plus tard
selon la feuille de route : well/chimney enchaines, briques fragiles,
obstacles fantomes, etc.)
"""
import re
import sys
from pathlib import Path

COLS = 20
ROWS = 15
BASE_ROW = 11
PATH = Path(__file__).parent.parent / "hand_levels.txt"


class Grid:
    def __init__(self):
        self.g = [['.'] * COLS for _ in range(ROWS)]
        self.anchors = []

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
    grid = Grid()
    row = BASE_ROW
    col = 0
    first = platforms[0]
    grid.putH(col, col + first['w'] - 1, row, '#')
    grid.putH(col, col + first['w'] - 1, row + 1, '#')
    grid.g[row - 1][col + 1] = 'S'
    grid.anchors.append((col + first['w'] // 2, row))
    col += first['w']

    last_c0 = last_c1 = last_row = row
    for p in platforms[1:]:
        gap = p['gap']
        row = clamp(row + p['dr'], 3, 13)
        c0 = col + gap
        c1 = c0 + p['w'] - 1
        if c1 > COLS - 1:
            c1 = COLS - 1
            if c1 - c0 < 1:
                break
        tile = 'C' if p.get('hazard') == 'crumble' else '#'
        grid.putH(c0, c1, row, tile)
        if p.get('hazard') == 'spike':
            grid.g[row - 1][(c0 + c1) // 2] = '^'
        grid.anchors.append(((c0 + c1) // 2, row))
        col = c1 + 1
        last_c0, last_c1, last_row = c0, c1, row

    mid = (last_c0 + last_c1) // 2
    grid.g[last_row - 1][mid] = 'G'

    mids = grid.anchors[1:-1] if len(grid.anchors) > 2 else grid.anchors
    if mids:
        idxs = sorted(set([0, len(mids) // 2, len(mids) - 1]))
        for i in idxs[:3]:
            c, r = mids[i]
            grid.star_above(c, r)
    return grid.dump()


# 10 layouts distincts (largeurs/gaps/denivelles varies a la main) :
DECADE1 = {
    0: [  # intro douce : plat, gaps courts
        {'w': 4}, {'gap': 2, 'w': 4, 'dr': 0}, {'gap': 2, 'w': 4, 'dr': 0}, {'gap': 2, 'w': 4, 'dr': 0},
    ],
    1: [  # escalier qui monte
        {'w': 4}, {'gap': 2, 'w': 3, 'dr': -1}, {'gap': 2, 'w': 3, 'dr': -1},
        {'gap': 2, 'w': 3, 'dr': -1}, {'gap': 2, 'w': 4, 'dr': -1},
    ],
    2: [  # vallee : descend puis remonte
        {'w': 4}, {'gap': 2, 'w': 3, 'dr': 2}, {'gap': 2, 'w': 3, 'dr': 1},
        {'gap': 2, 'w': 3, 'dr': -1}, {'gap': 2, 'w': 4, 'dr': -2},
    ],
    3: [  # gaps larges, peu de plateformes, plateformes generales
        {'w': 5}, {'gap': 4, 'w': 3, 'dr': -1}, {'gap': 4, 'w': 3, 'dr': 1}, {'gap': 3, 'w': 5, 'dr': 0},
    ],
    4: [  # beaucoup de petites plateformes etroites
        {'w': 3}, {'gap': 2, 'w': 2, 'dr': 1}, {'gap': 2, 'w': 2, 'dr': -1}, {'gap': 2, 'w': 2, 'dr': 1},
        {'gap': 2, 'w': 2, 'dr': -1}, {'gap': 2, 'w': 2, 'dr': 1}, {'gap': 2, 'w': 3, 'dr': 0},
    ],
    5: [  # zigzag ample (plus de denivele)
        {'w': 4}, {'gap': 3, 'w': 3, 'dr': -2}, {'gap': 3, 'w': 3, 'dr': 2},
        {'gap': 3, 'w': 3, 'dr': -2}, {'gap': 3, 'w': 4, 'dr': 2},
    ],
    6: [  # longue descente
        {'w': 4}, {'gap': 2, 'w': 4, 'dr': 1}, {'gap': 2, 'w': 4, 'dr': 1},
        {'gap': 2, 'w': 4, 'dr': 1}, {'gap': 2, 'w': 4, 'dr': 1},
    ],
    7: [  # alternance large/etroit
        {'w': 5}, {'gap': 2, 'w': 2, 'dr': -1}, {'gap': 2, 'w': 5, 'dr': 1},
        {'gap': 2, 'w': 2, 'dr': -1}, {'gap': 2, 'w': 5, 'dr': 0},
    ],
    8: [  # gaps proches du maximum sur (presque) toutes les plateformes
        {'w': 4}, {'gap': 4, 'w': 3, 'dr': 1}, {'gap': 4, 'w': 3, 'dr': -1}, {'gap': 4, 'w': 4, 'dr': 0},
    ],
    9: [  # "final" de la dizaine : cumule variete de largeurs, gaps et denivele
        {'w': 4}, {'gap': 3, 'w': 2, 'dr': -2}, {'gap': 2, 'w': 4, 'dr': 1}, {'gap': 4, 'w': 2, 'dr': -1},
        {'gap': 2, 'w': 3, 'dr': 2}, {'gap': 3, 'w': 2, 'dr': -2}, {'gap': 2, 'w': 5, 'dr': 1},
    ],
}


def load_existing(path):
    """Renvoie {n: [rows]} pour tout ce qui est deja dans hand_levels.txt."""
    levels = {}
    cur = None
    cur_n = None
    for raw in Path(path).read_text(encoding="utf-8").splitlines():
        line = raw.rstrip()
        m = re.match(r"LEVEL (\d+)\s*$", line)
        if m:
            cur_n = int(m.group(1)); cur = []
            continue
        if line.strip() == "END":
            if cur is not None:
                levels[cur_n] = cur
            cur = None
            continue
        if cur is not None:
            cur.append(line)
    return levels


def main():
    existing = load_existing(PATH) if PATH.exists() else {}
    for n, platforms in DECADE1.items():
        existing[n] = build_level(platforms)

    out = []
    out.append("# Niveaux dessines a la main.")
    out.append("# Dizaine 1 (0-9) : integralement dessinee a la main, layouts varies (pas de gabarit repete).")
    out.append("# Dizaines suivantes (.0/.1/.2 de chaque dizaine, ex 10,11,12,20,21,22...) : premier jet,")
    out.append("# a retravailler selon la feuille de route (well/chimney enchaines, briques fragiles,")
    out.append("# obstacles fantomes...).")
    out.append("# Format : 'LEVEL <n>' puis des rangees de 20 caracteres exactement, puis 'END'.")
    out.append("# Tuiles : # bloc | C plateforme fragile | ^ pic fixe | S depart | G drapeau | * etoile | . vide")
    out.append("# Relu automatiquement par gen_hand_levels.py a chaque compilation.")
    out.append("")
    for n in sorted(existing):
        out.append(f"LEVEL {n}")
        out.extend(existing[n])
        out.append("END")
        out.append("")
    PATH.write_text("\n".join(out), encoding="utf-8")
    print(f"hand_levels.txt reecrit : {len(existing)} niveaux (dizaine 1 remplacee par 10 layouts varies).", file=sys.stderr)


if __name__ == "__main__":
    main()
