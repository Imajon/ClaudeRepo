"""Esquisse SVG de l'ajoure C : PCB B + patte oblongue detachable (facon Pocket Operator) attachee aux 3 pointes du haut,
trou oblong pour crochet de rayon, bord cuivre + stries prolongees sur la patte, pastilles GND a cote des pastilles de fil. Usage : python make_accroche_svg.py"""
import math, re
from shapely.geometry import Polygon, Point, box
from shapely.ops import unary_union
import make_polygon_ajoure as M

s = open("pcb-polygon-ajoure-B/c3-tft-meatboy-polygon-ajoure-B.kicad_pcb", encoding="utf-8").read()
gp = lambda ly: [Polygon([tuple(map(float, m)) for m in re.findall(r"\(xy ([-\d.]+) ([-\d.]+)\)", b)])
                 for b in re.findall(r"\n\t\(gr_poly.*?\n\t\)", s, re.S) if f'"{ly}"' in b and "(fill yes)" in b or ly == "Edge.Cuts" and f'"{ly}"' in b]
edges = sorted(gp("Edge.Cuts"), key=lambda p: -p.area)
WIRE = [(x, y, side, nm) for x, y, side, nm in M.PADS_L]           # pastilles de fil (extremites gauche / droite)
GNDP = [(x + (5.0 if side == "L" else -5.0), y) for x, y, side, _ in WIRE]   # pastille GND a cote de chacune (3,8 mm, 1,2 mm d'ecart)
edges = edges[:1] + [h for h in edges[1:] if min(h.distance(Point(g)) for g in GNDP) > 1.9 + 2]   # ajours trop pres : retires
board = edges[0].difference(unary_union(edges[1:]))
stripes = gp("F.Cu")

PX, TIP = 56.69, 1.5                               # pointe du milieu du bord haut
SLOPE = (9.11 - 1.5) / (56.69 - 42.87)             # pente des cotes de la pointe
NW = 2.0                                           # 3 attaches de 2 mm
BRK = TIP + NW / 2 * SLOPE                         # cassure la ou la pointe fait 2 mm de large (pointe tronquee de 0,55 mm, sans rebord)
TOP, TBOT = -9.75, 1.0                             # patte oblongue couchee (facon Pocket Operator) : haut, bas
PEAKS = [29.06, PX, 84.29]                         # 3 attaches : les 3 pointes du haut (tronquees a BRK)
TW = 66.0
obround = lambda x, y, w, h: box(x - (w - h) / 2, y, x + (w - h) / 2, y).buffer(h / 2)   # oblong horizontal centre (x, y), w x h
ob = obround(PX, (TOP + TBOT) / 2, TW, TBOT - TOP)
necks = unary_union([box(x - NW / 2, TBOT - 3, x + NW / 2, BRK) for x in PEAKS])
tab = ob.union(necks)                              # patte entierement arrondie + 3 cous droits
cut = box(0, -50, 120, BRK)                         # tout ce qui est au-dessus de BRK part avec la patte
board = board.difference(cut)
SLW, SLH = 26.0, 4.0
slot = obround(PX, (TOP + TBOT) / 2, SLW, SLH)      # trou d'accroche oblong 26 x 4 mm
DOME = 3.5                                         # demi-cercle vers le haut au centre du trou (facon trou euro)
slot = slot.union(Point(PX, (TOP + TBOT) / 2).buffer(DOME).intersection(box(PX - DOME, TOP, PX + DOME, (TOP + TBOT) / 2)))
NOTCH_R, BITE_R, BITE_P = .35, .25, .8             # petites encoches aux 2 bouts de la cassure (amorces), pas de perforations sur 2 mm
notches = [Point(x + sx * NW / 2, BRK).buffer(NOTCH_R) for x in PEAKS for sx in (-1, 1)]
bites = []
BITE_Y, BW, cx, cy = BRK, NW - 3, PX, TOP + 17
part = unary_union([board, tab]).difference(unary_union(bites + notches + [slot]))
from shapely import affinity
EDGE_CLR, RIM, GAP = .4, 1.0, .6                   # bord cuivre de la patte : 1 mm, a 0,4 mm du bord fraise ; 0,6 mm avant les stries
rim = ob.buffer(-EDGE_CLR).difference(ob.buffer(-EDGE_CLR - RIM))
zone = ob.buffer(-EDGE_CLR - RIM - GAP).difference(slot.buffer(GAP))
rz = affinity.rotate(zone, 45, origin=(0, 0))      # meme trame que les stries du PCB : 2 mm a 45 deg, pas 4 mm, meme phase
tab_stripes = []
for k in range(-60, 60):
    for g in getattr(rz.intersection(box(-300, k * 4.0, 300, k * 4.0 + 2.0)), "geoms", [rz.intersection(box(-300, k * 4.0, 300, k * 4.0 + 2.0))]):
        for r in getattr(g.buffer(-.3).buffer(.3), "geoms", [g.buffer(-.3).buffer(.3)]):   # comme sur le PCB : pas d'eclats < 0,6 mm
            if r.geom_type == "Polygon" and r.area > 3:
                tab_stripes.append(affinity.rotate(r, -45, origin=(0, 0)))

x0, y0, x1, y1 = part.bounds
x0, y0, x1, y1 = x0 - 6, y0 - 6, x1 + 6, y1 + 10
P = lambda g: "".join(M.path(p) for p in getattr(g, "geoms", [g]))
o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="100%" viewBox="{x0:.1f} {y0:.1f} {x1 - x0:.1f} {y1 - y0:.1f}">', M.DEFS,
     f'<rect x="{x0}" y="{y0}" width="{x1 - x0}" height="{y1 - y0}" fill="#d9dde3"/>',
     f'<path fill="url(#pcb)" stroke="#0c3a20" stroke-width=".2" fill-rule="evenodd" d="{P(part)}'
     + "".join(f"M{x - 1.6},{y}a1.6,1.6 0 1,0 3.2,0a1.6,1.6 0 1,0 -3.2,0Z" for x, y in M.MOUNT) + '"/>',
     f'<path fill="url(#gold)" fill-rule="evenodd" d="{"".join(P(g) for g in stripes + tab_stripes + [rim])}"/>',
     "".join(f'<circle cx="{x}" cy="{y}" r="1.9" fill="url(#gold)"/><circle cx="{x}" cy="{y}" r=".6" fill="#d9dde3"/>' for x, y in [(a, b) for a, b, *_ in WIRE] + GNDP),
     "".join(f'<text x="{x + (.5 if sd == "L" else -.5)}" y="{y + (-3.0 if k % 2 == 0 else 3.0) + .4}" {M.S} font-size="1.25" text-anchor="middle">{t}</text>'
             for k, (x, y, sd, nm) in enumerate(WIRE) for t, x in ((nm, x), ("GND", x + (5.0 if sd == "L" else -5.0)))),
     "".join(f'<path d="M{x - BW / 2 - 1.5},{BITE_Y} H{x + BW / 2 + 1.5}" stroke="#e00" stroke-width=".2" stroke-dasharray=".6 .4" fill="none"/>' for x in PEAKS),
     f'<text x="{(x0 + x1) / 2}" y="{y1 - 3}" font-family="Arial" font-size="2.2" fill="#2c3038" text-anchor="middle">'
     f'Ajouré C (esquisse) : patte oblongue {TW:.0f} × {BRK - TOP:.0f} mm, trou {SLW:.0f} × {SLH:.0f} mm + demi-cercle R{DOME}, 3 attaches de {NW:.0f} mm</text>'
     f'<text x="{(x0 + x1) / 2}" y="{y1 - .5}" font-family="Arial" font-size="2.2" fill="#c00" text-anchor="middle">pointillés rouges = lignes de cassure (pointes du haut tronquées de {BRK - TIP:.2f} mm)</text>',
     "</svg>"]
open("polygon-ajoure-C-accroche.svg", "w", encoding="utf-8").write("\n".join(o))
print("ok", [round(v, 1) for v in part.bounds])
