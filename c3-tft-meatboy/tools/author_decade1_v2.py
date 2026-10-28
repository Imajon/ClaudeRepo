#!/usr/bin/env python3
"""Script ponctuel (pas appele par le build) : regenere les niveaux 0-9 de
hand_levels.txt avec une vraie progression -- de plus en plus de plateformes,
un drapeau de plus en plus haut (retour du 2026-09-27). Remplace le bloc
LEVEL 0 .. LEVEL 9 existant, laisse le reste du fichier intact.

Usage: python author_decade1_v2.py (ecrit directement hand_levels.txt)
"""
import re
from pathlib import Path

COLS = 20


def gen_level(n):
    levH = 15 if n <= 3 else 15 + (n - 3) * 2
    num_platforms = 4 + n
    rows = [['.'] * COLS for _ in range(levH)]
    bottom = levH - 2
    for c in range(0, 4):
        rows[bottom][c] = '#'
        rows[bottom + 1][c] = '#'
    rows[bottom - 1][1] = 'S'

    cur_row, cur_col, direction = bottom, 3, 1
    steps = []
    for i in range(num_platforms):
        rise = 2 if (i % 4 == 3) else 1
        cur_row -= rise
        if cur_row < 1:
            cur_row = 1
        width = 3 if i < num_platforms - 1 else 4
        gap = 3 if i % 2 == 0 else 4
        nxt = cur_col + direction * gap
        if nxt < 1 or nxt + width > COLS - 1:
            direction = -direction
            nxt = cur_col + direction * gap
        nxt = max(1, min(nxt, COLS - 1 - width))
        cur_col = nxt
        for c in range(cur_col, min(cur_col + width, COLS)):
            rows[cur_row][c] = '#'
        steps.append((cur_row, cur_col, width))
        if cur_row <= 1:
            break

    gr, gc, gw = steps[-1]
    rows[gr - 1][min(gc + gw - 2, COLS - 1)] = 'G'
    for i, (r, c, w) in enumerate(steps):
        if i > 0 and i % 3 == 1 and r - 2 >= 0:
            rows[r - 2][min(c + 1, COLS - 1)] = '*'

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
    print("hand_levels.txt : dizaine 1 (niveaux 0-9) regeneree avec progression de hauteur/plateformes.")


if __name__ == "__main__":
    main()
