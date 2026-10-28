#!/usr/bin/env python3
"""Convertit hand_levels.txt en hand_levels_data.h (tableau C++ HAND_LEVELS[]).
Appele automatiquement par compiler-upload-c3-tft-meatboy.bat avant chaque
compilation : modifie hand_levels.txt, le .h est regenere tout seul.

Format de hand_levels.txt : voir l'entete de ce fichier (genere par
author_hand_levels.py, puis a redessiner/ajuster a la main).
"""
import re
import sys
from pathlib import Path

COLS = 20
HAND_MAXH = 42   # doit rester <= WROWS (80) cote .ino -- releve 22->28->36->42 (dizaine 1 "squishy" : rebonds qui montent haut)


def parse(path):
    levels = []
    cur = None
    cur_n = None
    for lineno, raw in enumerate(Path(path).read_text(encoding="utf-8").splitlines(), 1):
        line = raw.rstrip("\r\n")
        m = re.match(r"LEVEL (\d+)\s*$", line)
        if m:
            if cur is not None:
                print(f"hand_levels.txt:{lineno}: 'LEVEL {cur_n}' jamais ferme par END", file=sys.stderr)
            cur_n = int(m.group(1))
            cur = []
            continue
        if line.strip() == "END":
            if cur is None:
                print(f"hand_levels.txt:{lineno}: 'END' sans 'LEVEL' correspondant, ignore", file=sys.stderr)
                continue
            ok = True
            for r, row in enumerate(cur):
                if len(row) != COLS:
                    print(f"hand_levels.txt: niveau {cur_n} rangee {r} fait {len(row)} caracteres (attendu {COLS})", file=sys.stderr)
                    ok = False
            joined = "".join(cur)
            if joined.count("S") != 1:
                print(f"hand_levels.txt: niveau {cur_n} doit avoir exactement un 'S' (trouve {joined.count('S')})", file=sys.stderr)
                ok = False
            if joined.count("G") != 1:
                print(f"hand_levels.txt: niveau {cur_n} doit avoir exactement un 'G' (trouve {joined.count('G')})", file=sys.stderr)
                ok = False
            if len(cur) > HAND_MAXH:
                print(f"hand_levels.txt: niveau {cur_n} trop haut ({len(cur)} rangees, max {HAND_MAXH})", file=sys.stderr)
                ok = False
            if cur_n // 10 == 3:
                print(f"hand_levels.txt: niveau {cur_n} ignore (dizaine 31-40 geree par genPhantom() dans le .ino, pas dessinee a la main)", file=sys.stderr)
                ok = False
            elif cur_n >= 10 and cur_n % 10 >= 3:
                print(f"hand_levels.txt: niveau {cur_n} ignore (a partir de la dizaine 2, seuls .0/.1/.2 sont dessines a la main)", file=sys.stderr)
                ok = False
            if ok:
                levels.append((cur_n, cur))
            cur = None
            continue
        if cur is not None:
            cur.append(line)
    if cur is not None:
        print(f"hand_levels.txt: 'LEVEL {cur_n}' jamais ferme par END (fin de fichier)", file=sys.stderr)
    return levels


def esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


def generate(levels):
    lines = [
        "// Fichier GENERE AUTOMATIQUEMENT par gen_hand_levels.py a partir de hand_levels.txt.",
        "// Ne pas editer a la main : modifier hand_levels.txt puis recompiler.",
        "#pragma once",
        f"#define HAND_MAXH {HAND_MAXH}",
        "struct HandLevel { int n; int h; const char* rows[HAND_MAXH]; };",
        f"#define N_HAND {len(levels)}",
        "const HandLevel HAND_LEVELS[N_HAND] = {",
    ]
    for (n, rows) in levels:
        padded = list(rows) + [""] * (HAND_MAXH - len(rows))
        rows_str = ", ".join(f'"{esc(r)}"' for r in padded)
        lines.append(f'  {{ {n}, {len(rows)}, {{ {rows_str} }} }},')
    lines.append("};")
    return "\n".join(lines) + "\n"


def main():
    src = Path(__file__).parent / "hand_levels.txt"
    dst = Path(__file__).parent / "hand_levels_data.h"
    levels = parse(src)
    levels.sort(key=lambda t: t[0])
    dst.write_text(generate(levels), encoding="utf-8")
    print(f"hand_levels_data.h regenere ({len(levels)} niveaux dessines a la main).", file=sys.stderr)


if __name__ == "__main__":
    main()
