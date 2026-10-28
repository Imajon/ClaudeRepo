#!/usr/bin/env python3
"""Parse the level dump captured from dump-levels.ino and render SVG(s)
showing the structure of all 100 procedurally-generated levels.

Usage: python render_svg.py levels_dump.log
Outputs: levels_atlas.svg (overview grid of all 100 levels)
         levels_detail.svg (one bigger example per generator "kind")
"""
import re
import sys
from pathlib import Path

TILE = 16
COLS = 20

KIND_COLOR = {
    "chain":   "#3a7bd5",
    "stairs":  "#2fb56b",
    "islands": "#c99a2f",
    "chimney": "#8e44ad",
    "well":    "#c0392b",
    "climb":   "#16a2a2",
    "traps":   "#d5455a",
    "hand":    "#f2f2f2",
    "phantom": "#8899ff",
    "push":          "#d8a040",
    "phantom_tower": "#8899ff",
    # dizaine 11-20 (colonnes empilees)
    "columns":        "#8e44ad",
    # dizaine 21-30 (briques fragiles rouges)
    "fragile_well":   "#ff5560",
    "fragile_traps":  "#ff5560",
    "fragile_stairs": "#ff5560",
    # dizaine 41-50 (cumul final)
    "hard_phantom":   "#5566ff",
    "hard_fragile":   "#ff2a3a",
    "hard_chainwell": "#ff2a3a",
}
TILE_COLOR = {
    '#': "#5a6478",   # bloc solide
    'C': "#e0333f",   # plateforme fragile -- rouge dedie (COL_FRAG dans le .ino)
    '^': "#e0e4ea",   # pic fixe
    'S': "#3ccf6e",   # depart
    'G': "#f2c744",   # drapeau (position reelle)
    'B': "#d8a040",   # brique a pousser
    'R': "#5aeb8c",   # ressort/tremplin (dizaine 1 "squishy") -- COL_BOUNCE dans le .ino
    'W': "#eaeaea",   # marqueur de tronconneuse (hand_levels.txt) -- pas une tuile solide, juste pour la review
}
# H/X : invisibles EN JEU (c'est le piege), mais affiches ici en outil de
# review pour que le level designer sache ou ils sont.
GHOST_TILE_COLOR = {
    'H': "#ff5560",   # trou invisible (rendu comme '#' en jeu)
    'X': "#ff9d3c",   # pic invisible (rien du tout en jeu)
}


def parse(path):
    levels = []
    cur = None
    with open(path, encoding="utf-8", errors="ignore") as f:
        for raw in f:
            line = raw.rstrip("\r\n")
            m = re.match(r"LEVEL (\d+) (\w+) H(\d+)", line)
            if m:
                cur = {
                    "n": int(m.group(1)), "kind": m.group(2), "h": int(m.group(3)),
                    "rows": [], "saws": [], "traps": [], "stars": [], "decoys": [],
                }
                levels.append(cur)
                continue
            if cur is None:
                continue
            if line == "END":
                cur = None
                continue
            if line.startswith("SAW "):
                _, axis, fixed, lo, hi = line.split()
                cur["saws"].append((int(axis), float(fixed), float(lo), float(hi)))
                continue
            if line.startswith("TRAP "):
                parts = line.split()
                kind, tx, ty = parts[1], parts[2], parts[3]
                hidden = parts[4] if len(parts) > 4 else "0"
                cur["traps"].append((int(kind), int(tx), int(ty), bool(int(hidden))))
                continue
            if line.startswith("STAR "):
                _, tx, ty = line.split()
                cur["stars"].append((int(tx), int(ty)))
                continue
            if line.startswith("DECOY "):
                _, tx, ty = line.split()
                cur["decoys"].append((int(tx), int(ty)))
                continue
            if len(cur["rows"]) < cur["h"] and len(line) >= 1:
                cur["rows"].append(line[:COLS].ljust(COLS, '.'))
    return levels


def render_atlas(levels, cell=4):
    cols_grid = 10
    pad = 10
    label_h = 12
    max_h = max(l["h"] for l in levels)
    cw = COLS * cell
    ch = max_h * cell
    tile_w = cw + pad
    tile_h = ch + label_h + pad
    rows_grid = (len(levels) + cols_grid - 1) // cols_grid
    W = cols_grid * tile_w + pad
    H = rows_grid * tile_h + pad

    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" '
           f'font-family="monospace" font-size="9">']
    svg.append(f'<rect x="0" y="0" width="{W}" height="{H}" fill="#12141c"/>')

    for i, lvl in enumerate(levels):
        gx = pad + (i % cols_grid) * tile_w
        gy = pad + (i // cols_grid) * tile_h
        kcol = KIND_COLOR.get(lvl["kind"], "#888")
        svg.append(f'<rect x="{gx-3}" y="{gy-3}" width="{cw+6}" height="{ch+label_h+6}" '
                    f'rx="3" fill="#1b1f2c" stroke="{kcol}" stroke-width="1.5"/>')
        svg.append(f'<text x="{gx}" y="{gy+label_h-3}" fill="{kcol}">'
                    f'{lvl["n"]+1:>3} {lvl["kind"]}</text>')
        oy = gy + label_h
        for r, row in enumerate(lvl["rows"]):
            for c, ch_ in enumerate(row):
                ghost = ch_ in GHOST_TILE_COLOR
                col = GHOST_TILE_COLOR.get(ch_) or TILE_COLOR.get(ch_)
                if not col:
                    continue
                extra = ' fill-opacity="0.55" stroke="#fff" stroke-width="0.5" stroke-dasharray="1,1"' if ghost else ''
                svg.append(f'<rect x="{gx+c*cell}" y="{oy+r*cell}" width="{cell}" height="{cell}" fill="{col}"{extra}/>')
        for (kind, tx, ty, hidden) in lvl["traps"]:
            col = "#ff5560" if kind == 0 else "#ff9d3c"
            extra = ' fill-opacity="0.55" stroke="#fff" stroke-width="0.5" stroke-dasharray="1,1"' if hidden else ''
            svg.append(f'<circle cx="{gx+tx*cell+cell/2}" cy="{oy+ty*cell+cell/2}" r="{cell*0.6}" fill="{col}"{extra}/>')
        for (axis, fixed, lo, hi) in lvl["saws"]:
            if axis == 1:  # vertical, fixed = x en px
                cx = fixed / TILE
                cy = (lo + hi) / 2 / TILE
            else:
                cx = (lo + hi) / 2 / TILE
                cy = fixed / TILE
            svg.append(f'<circle cx="{gx+cx*cell}" cy="{oy+cy*cell}" r="{cell*0.7}" '
                        f'fill="none" stroke="#eaeaea" stroke-width="0.8"/>')
        for (tx, ty) in lvl["stars"]:
            svg.append(f'<circle cx="{gx+tx*cell+cell/2}" cy="{oy+ty*cell+cell/2}" r="{cell*0.45}" fill="#ffe066"/>')
        for (tx, ty) in lvl["decoys"]:
            svg.append(f'<circle cx="{gx+tx*cell+cell/2}" cy="{oy+ty*cell+cell/2}" r="{cell*0.6}" '
                        f'fill="none" stroke="#ffe066" stroke-width="0.8" stroke-dasharray="1,1"/>')

    # legende
    ly = H - pad
    svg.insert(1, "")  # placeholder, legend appended at end instead
    svg.append(f'<text x="{pad}" y="{H-4}" fill="#aab" font-size="8">'
                '#=bloc  C=fragile  ^=pic  cercle plein rouge/orange=piege  '
                'cercle creux=scie  point jaune=etoile  pointille jaune=leurre drapeau</text>')
    svg.append('</svg>')
    return "\n".join(svg)


def render_detail(levels, cell=10):
    picked = {}
    for lvl in levels:
        picked.setdefault(lvl["kind"], lvl)  # premier rencontre de chaque type
    for lvl in levels:  # pour "phantom", prefere un niveau ou les pieges sont deja invisibles (pos>=2)
        if lvl["kind"] == "phantom" and lvl["n"] % 10 >= 2:
            picked["phantom"] = lvl
            break
    order = ["hand", "chain", "stairs", "islands", "chimney", "well", "climb", "traps", "phantom"]
    picked_list = [picked[k] for k in order if k in picked]

    pad = 16
    label_h = 16
    max_h = max(l["h"] for l in picked_list)
    cw = COLS * cell
    ch = max_h * cell
    tile_w = cw + pad
    tile_h = ch + label_h + pad
    cols_grid = 4
    rows_grid = (len(picked_list) + cols_grid - 1) // cols_grid
    W = cols_grid * tile_w + pad
    H = rows_grid * tile_h + pad

    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" '
           f'font-family="monospace" font-size="12">']
    svg.append(f'<rect x="0" y="0" width="{W}" height="{H}" fill="#12141c"/>')

    for i, lvl in enumerate(picked_list):
        gx = pad + (i % cols_grid) * tile_w
        gy = pad + (i // cols_grid) * tile_h
        kcol = KIND_COLOR.get(lvl["kind"], "#888")
        svg.append(f'<rect x="{gx-4}" y="{gy-4}" width="{cw+8}" height="{ch+label_h+8}" '
                    f'rx="4" fill="#1b1f2c" stroke="{kcol}" stroke-width="2"/>')
        svg.append(f'<text x="{gx}" y="{gy+label_h-4}" fill="{kcol}">'
                    f'niveau {lvl["n"]+1} - {lvl["kind"]}</text>')
        oy = gy + label_h
        for r, row in enumerate(lvl["rows"]):
            for c, ch_ in enumerate(row):
                ghost = ch_ in GHOST_TILE_COLOR
                col = GHOST_TILE_COLOR.get(ch_) or TILE_COLOR.get(ch_)
                if not col:
                    continue
                extra = ' fill-opacity="0.55" stroke="#fff" stroke-width="0.7" stroke-dasharray="1.5,1.5"' if ghost else ''
                svg.append(f'<rect x="{gx+c*cell}" y="{oy+r*cell}" width="{cell}" height="{cell}" fill="{col}"{extra}/>')
        for (kind, tx, ty, hidden) in lvl["traps"]:
            col = "#ff5560" if kind == 0 else "#ff9d3c"
            extra = ' fill-opacity="0.55" stroke="#fff" stroke-width="0.7" stroke-dasharray="1.5,1.5"' if hidden else ''
            svg.append(f'<circle cx="{gx+tx*cell+cell/2}" cy="{oy+ty*cell+cell/2}" r="{cell*0.55}" fill="{col}"{extra}/>')
        for (axis, fixed, lo, hi) in lvl["saws"]:
            if axis == 1:
                cx, cy = fixed / TILE, (lo + hi) / 2 / TILE
                y0, y1 = lo / TILE, hi / TILE
                svg.append(f'<line x1="{gx+cx*cell}" y1="{oy+y0*cell}" x2="{gx+cx*cell}" y2="{oy+y1*cell}" '
                            f'stroke="#eaeaea" stroke-width="1" stroke-dasharray="2,2"/>')
            else:
                cx, cy = (lo + hi) / 2 / TILE, fixed / TILE
                x0, x1 = lo / TILE, hi / TILE
                svg.append(f'<line x1="{gx+x0*cell}" y1="{oy+cy*cell}" x2="{gx+x1*cell}" y2="{oy+cy*cell}" '
                            f'stroke="#eaeaea" stroke-width="1" stroke-dasharray="2,2"/>')
            svg.append(f'<circle cx="{gx+cx*cell}" cy="{oy+cy*cell}" r="{cell*0.6}" '
                        f'fill="none" stroke="#eaeaea" stroke-width="1.2"/>')
        for (tx, ty) in lvl["stars"]:
            svg.append(f'<circle cx="{gx+tx*cell+cell/2}" cy="{oy+ty*cell+cell/2}" r="{cell*0.4}" fill="#ffe066"/>')
        for (tx, ty) in lvl["decoys"]:
            svg.append(f'<circle cx="{gx+tx*cell+cell/2}" cy="{oy+ty*cell+cell/2}" r="{cell*0.55}" '
                        f'fill="none" stroke="#ffe066" stroke-width="1" stroke-dasharray="1.5,1.5"/>')

    svg.append('</svg>')
    return "\n".join(svg)


if __name__ == "__main__":
    src = sys.argv[1] if len(sys.argv) > 1 else "levels_dump.log"
    levels = parse(src)
    print(f"{len(levels)} niveaux parses", file=sys.stderr)
    out_dir = Path(src).parent
    (out_dir / "levels_atlas.svg").write_text(render_atlas(levels), encoding="utf-8")
    (out_dir / "levels_detail.svg").write_text(render_detail(levels), encoding="utf-8")
    print("OK", file=sys.stderr)
