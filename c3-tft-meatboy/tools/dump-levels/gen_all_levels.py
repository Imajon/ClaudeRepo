#!/usr/bin/env python3
"""Combine les niveaux dessines a la main (../hand_levels.txt, n%10<3) et les
niveaux procenduraux (gen_levels.py) pour produire un levels_dump.log complet
(meme format que dump-levels.ino) -> a passer a render_svg.py.

Usage: python gen_all_levels.py > levels_dump.log
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from gen_levels import gen_level, kind_of, fill_stars, NUM_LEVELS, COLS  # noqa: E402

HAND_TXT = Path(__file__).parent.parent.parent / "hand_levels.txt"


def parse_hand(path):
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


def is_hand(n):
    """Mirroir de la logique de dispatch de genLevel() dans le .ino : quels
    niveaux sont dessines a la main plutot que generes proceduralement."""
    if n < 10:
        return True                  # dizaine 1 : entierement a la main
    if 20 <= n < 30:
        return n % 10 < 3            # dizaine 3 : seuls .0/.1/.2 (20-22)
    if n >= 50:
        return n % 10 < 3            # dizaines 6-10 : ancien schema .0/.1/.2
    return False                     # dizaines 2, 4, 5 : entierement procedurales


def main():
    hand = parse_hand(HAND_TXT)
    out = sys.stdout
    out.write("BEGIN\n")
    for n in range(NUM_LEVELS):
        if is_hand(n) and n in hand:
            rows = hand[n]
            out.write(f"LEVEL {n} hand H{len(rows)}\n")
            for r in rows:
                out.write(r + "\n")
            stars = [(c, r) for r, row in enumerate(rows) for c, ch in enumerate(row) if ch == '*'][:3]
            grid = [list(row.ljust(COLS, '.').replace('*', '.')) for row in rows]
            fill_stars(grid, len(rows), stars, [])   # miroir de fillStars() du .ino
            for (c, r) in stars:
                out.write(f"STAR {c} {r}\n")
            out.write("END\n")
        else:
            lvl, kind = gen_level(n)
            out.write(f"LEVEL {n} {kind} H{lvl.levH}\n")
            for r in range(lvl.levH):
                out.write("".join(lvl.grid[r]) + "\n")
            for (axis, fixed, lo, hi) in lvl.saws:
                out.write(f"SAW {axis} {fixed:.1f} {lo:.1f} {hi:.1f}\n")
            for (kind_, tx, ty, hidden) in lvl.traps:
                out.write(f"TRAP {kind_} {tx} {ty} {int(hidden)}\n")
            for (tx, ty) in lvl.stars:
                out.write(f"STAR {tx} {ty}\n")
            for (tx, ty) in lvl.gp:
                out.write(f"DECOY {tx} {ty}\n")
            out.write("END\n")
    out.write("ALLDONE\n")


if __name__ == "__main__":
    main()
