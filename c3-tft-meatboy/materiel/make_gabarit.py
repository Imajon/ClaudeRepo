"""Gabarit SVG a l'echelle 1:1 (mm) du PCB : contour (+ ajours), 4 trous M3, carres aux boutons (5 mm petits, 10 mm SAUT).
Lu dans le .kicad_pcb. Usage : python make_gabarit.py [dossier-pcb]  (defaut pcb-polygon-ajoure-B)"""
import re, sys, glob

D = sys.argv[1] if len(sys.argv) > 1 else "pcb-polygon-ajoure-B"
s = open(glob.glob(f"{D}/*.kicad_pcb")[0], encoding="utf-8").read()
SQ = {"SW1": 5, "SW2": 5, "SW3": 10}                                   # SW3 = SAUT

polys = [[tuple(map(float, m)) for m in re.findall(r"\(xy ([-\d.]+) ([-\d.]+)\)", b)]
         for b in re.findall(r"\n\t\(gr_poly.*?\n\t\)", s, re.S) if '"Edge.Cuts"' in b]
polys.sort(key=lambda p: -(max(x for x, _ in p) - min(x for x, _ in p)))  # contour d'abord, puis ajours
holes, squares = [], []
for b in re.findall(r"\n\t\(footprint.*?\n\t\)(?=\n)", s, re.S):
    ref = re.search(r'"Reference" "([^"]*)"', b).group(1)
    X, Y = map(float, re.search(r"\n\t\t\(at ([-\d.]+) ([-\d.]+)", b).group(1, 2))
    pads = [tuple(map(float, re.search(r"\(at ([-\d.]+) ([-\d.]+)", p).groups())) for p in re.findall(r"\n\t\t\(pad .*?\n\t\t\)", b, re.S)]
    if ref.startswith("MTG"):
        holes.append((X, Y, float(re.search(r"\(drill ([\d.]+)", b).group(1)) / 2))
    elif ref in SQ:                                                    # centre = moyenne des pastilles (empreintes non tournees)
        squares.append((X + sum(x for x, _ in pads) / len(pads), Y + sum(y for _, y in pads) / len(pads), SQ[ref]))

x0, y0 = min(x for x, _ in polys[0]) - 1, min(y for _, y in polys[0]) - 1
x1, y1 = max(x for x, _ in polys[0]) + 1, max(y for _, y in polys[0]) + 1
pt = lambda p: " ".join(f"{x:.3f},{y:.3f}" for x, y in p)
o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{x1 - x0:.2f}mm" height="{y1 - y0:.2f}mm" viewBox="{x0:.2f} {y0:.2f} {x1 - x0:.2f} {y1 - y0:.2f}">',
     '<g fill="none" stroke="#000" stroke-width=".1">',
     f'<polygon id="contour" points="{pt(polys[0])}"/>',
     '<g id="ajours">' + "".join(f'<polygon points="{pt(p)}"/>' for p in polys[1:]) + "</g>",
     '<g id="trous" stroke="#d00">' + "".join(f'<circle cx="{x}" cy="{y}" r="{r}"/>' for x, y, r in holes) + "</g>",
     '<g id="boutons" stroke="#00c">' + "".join(f'<rect x="{x - c / 2:.3f}" y="{y - c / 2:.3f}" width="{c}" height="{c}"/>' for x, y, c in squares) + "</g>",
     "</g></svg>"]
out = f"{D}/gabarit-trous-boutons.svg"
open(out, "w", encoding="utf-8").write("\n".join(o))
print(out, len(holes), "trous,", len(squares), "carres,", len(polys) - 1, "ajours")
assert len(holes) == 4 and len(squares) == 3
