#!/usr/bin/env python3
"""Script ponctuel (pas appele par le build) : redessine les niveaux 0-9 de
hand_levels.txt en mode "squishy/rebondissant" (retour du 2026-09-28) --
remplace les marches par des ressorts 'R' (tuile ajoutee au .ino, rebond
BOUNCE_V bien plus fort qu'un saut normal ~3 tuiles -> ~5 tuiles par rebond).
Remplace le bloc LEVEL 0 .. LEVEL 9 existant, laisse le reste du fichier intact.

Usage: python author_decade1_squishy.py (ecrit directement hand_levels.txt)
"""
import re
from pathlib import Path

COLS = 20
SEG_H = 5          # rangees gagnees par rebond (un saut normal n'en franchit que ~3 -> le ressort est necessaire)
GAP = 4             # decalage horizontal entre deux plateformes


def gen_level(n):
    bounces = 1 + (n + 1) // 2   # 1,2,2,3,3,4,4,5,5,6 -- retour "niveau 2 identique au niveau 1" : decale, niveau2 a bien 2 plateformes (pas 1)
    levH = max(15, 8 + bounces * SEG_H)
    rows = [['.'] * COLS for _ in range(levH)]
    bottom = levH - 2
    plat_w = 5        # 4->5 : laisse 2 colonnes "#" libres (hors S/R) sur chaque plateforme pour fillStars()
    for c in range(0, plat_w):
        rows[bottom][c] = '#'
        rows[bottom + 1][c] = '#'
    rows[bottom - 1][1] = 'S'
    rows[bottom][plat_w - 1] = 'R'                   # 1er ressort : contigu au sol de depart, pas de saut a faire pour l'atteindre

    steps = [(bottom, 0, plat_w)]
    # direction/ecart varies avec n (pas juste avec bounces) -- deux niveaux du
    # meme nombre de rebonds (ex. 0 et 1) ne doivent pas etre des layouts identiques
    cur_row, cur_col, direction = bottom, plat_w - 1, 1 if n % 2 == 0 else -1
    gap = GAP + (n % 2)
    for i in range(bounces):
        cur_row -= SEG_H
        if cur_row < 2:
            cur_row = 2
        width = 5
        nxt = cur_col + direction * gap
        if nxt < 1 or nxt + width > COLS - 1:
            direction = -direction
            nxt = cur_col + direction * gap
        nxt = max(1, min(nxt, COLS - 1 - width))
        cur_col = nxt
        for c in range(cur_col, min(cur_col + width, COLS)):
            rows[cur_row][c] = '#'
        if i < bounces - 1:                          # pas sur la derniere (c'est la plateforme du drapeau)
            rows[cur_row][cur_col + width // 2] = 'R'
        steps.append((cur_row, cur_col, width))
        if cur_row <= 2:
            break

    gr, gc, gw = steps[-1]
    rows[gr - 1][min(gc + gw // 2, COLS - 1)] = 'G'
    for i, (r, c, w) in enumerate(steps):
        if 0 < i < len(steps) - 1 and r - 2 >= 0:
            rows[r - 2][min(c + 1, COLS - 1)] = '*'

    # detour non lineaire (retour : "moins lineaire") -- une plateforme bonus
    # hors du chemin principal, facultative, avec son etoile. A partir du
    # niveau 5 (n=4) elle porte aussi un pic sur un bord (risque/recompense) :
    # 2 cases sures pour atterrir, 1 case piegee.
    if len(steps) >= 3:
        br, bc, bw = steps[len(steps) // 2]
        side = 1 if n % 2 == 0 else -1
        dcol = max(1, min(bc + side * (bw + 2), COLS - 4))
        drow = br - 1
        if 2 <= drow < levH - 1 and rows[drow][dcol] == '.':
            for c in range(dcol, min(dcol + 3, COLS)):
                rows[drow][c] = '#'
            rows[drow - 1][dcol + 1] = '*'
            if n >= 4:
                rows[drow][min(dcol + 2, COLS - 1)] = '^'

    # scies et pics a partir du niveau 5 (n=4) -- 'W' = marqueur de
    # tronconneuse verticale (lu par loadHandLevel() dans le .ino, pas une
    # tuile solide), placee dans un ecart ouvert du chemin principal.
    if n >= 4 and len(steps) >= 3:
        r1, c1, _w1 = steps[1]
        r2, c2, _w2 = steps[2]
        saw_r = max(2, min(r1, r2) - 1)
        saw_c = max(1, min((c1 + c2) // 2, COLS - 2))
        if rows[saw_r][saw_c] == '.':
            rows[saw_r][saw_c] = 'W'

    return levH, [''.join(r) for r in rows]


def render(n):
    levH, rows = gen_level(n)
    lines = [f"LEVEL {n}"]
    lines.extend(rows)
    lines.append("END")
    lines.append("")
    return "\n".join(lines)


def main():
    path = Path(__file__).parent.parent / "hand_levels.txt"
    text = path.read_text(encoding="utf-8")
    block = "\n".join(render(n) for n in range(10)) + "\n"
    new_text = re.sub(r"LEVEL 0\n.*?LEVEL 9\n.*?END\n\n", block, text, flags=re.DOTALL)
    if new_text == text:
        raise SystemExit("substitution n'a rien change -- verifier le pattern")
    path.write_text(new_text, encoding="utf-8")
    print("hand_levels.txt : dizaine 1 (niveaux 0-9) redessinee en mode squishy/rebondissant (tuile 'R').")


if __name__ == "__main__":
    main()
