"""Genere pcb-polygon-tab/c3-tft-meatboy-polygon-tab.kicad_pcb (meme geometrie que polygon-tab-lettres.svg), route les pistes
(autorouteur maison, route_tab.py) puis exporte les Gerbers via kicad-cli. Usage : python make_kicad_tab.py
Cablage = celui du .ino / montage-c3-tft.svg."""
import os, subprocess, uuid, json, qrcode
from shapely.geometry import Polygon, box
from shapely.ops import unary_union
import make_polygon_ajoure as M
import route_tab

OUT_DIR = "pcb-polygon-tab"
NAME = "c3-tft-meatboy-polygon-tab"
KICAD_CLI = r"C:\Users\info\AppData\Local\Programs\KiCad\10.0\bin\kicad-cli.exe"
OY = 3.0                                    # decalage Y : tout en coordonnees positives dans KiCad
EDGE_CLEAR = .35                            # cuivre des lettres a >= 0,3 mm du bord de carte
uid = lambda: str(uuid.uuid4())
o = []
w = o.append
PADS = []                                   # pastilles (coordonnees de conception) pour le routage


def simple(p):
    """Polygone avec trous -> anneau unique (fente de liaison) : gr_poly n'accepte pas de trous."""
    ext = list(p.exterior.coords)[:-1]
    for h in p.interiors:
        hc = list(h.coords)[:-1]
        i, j = min(((i, j) for i in range(len(ext)) for j in range(len(hc))),
                   key=lambda t: (ext[t[0]][0] - hc[t[1]][0]) ** 2 + (ext[t[0]][1] - hc[t[1]][1]) ** 2)
        ext = ext[:i + 1] + hc[j:] + hc[:j + 1] + [ext[i]] + ext[i + 1:]
    return ext


def poly(pts, layer, fill=True, width=.1):
    xy = " ".join(f"(xy {x:.4f} {y + OY:.4f})" for x, y in pts)
    w(f'\t(gr_poly (pts {xy}) (stroke (width {width}) (type default)) (fill {"yes" if fill else "no"}) (layer "{layer}") (uuid "{uid()}"))')


def geoms(g):
    return list(g.geoms) if hasattr(g, "geoms") else [g]


def fill_layers(g, layers):
    for p in geoms(g):
        if p.is_empty or p.area < .01:
            continue
        for ly in layers:
            poly(simple(p), ly)


def text(s, x, y, layer="F.SilkS", size=1.2, just="center"):
    w(f'\t(gr_text "{s}" (at {x:.3f} {y + OY:.3f}) (layer "{layer}") (uuid "{uid()}") (effects (font (size {size} {size}) (thickness {size * .15:.3f})){"" if just == "center" else f" (justify {just})"}))')


def P(num, kind, shape, dx, dy, size, net=None, drill=None, smd_back=False):
    return dict(num=num, kind=kind, shape=shape, dx=dx, dy=dy, size=size, net=net, drill=drill, back=smd_back)


def footprint(name, ref, x, y, pads, layer="F.Cu"):
    w(f'\t(footprint "{name}" (layer "{layer}") (uuid "{uid()}") (at {x:.4f} {y + OY:.4f})')
    w(f'\t\t(property "Reference" "{ref}" (at 0 0) (layer "F.SilkS") (hide yes) (uuid "{uid()}") (effects (font (size 1 1) (thickness .15))))')
    w(f'\t\t(property "Value" "{name}" (at 0 0) (layer "F.Fab") (hide yes) (uuid "{uid()}") (effects (font (size 1 1) (thickness .15))))')
    for p in pads:
        L = '"B.Cu" "B.Mask" "B.Paste"' if p["back"] else ('"*.Cu" "*.Mask"' if p["kind"] != "smd" else '"F.Cu" "F.Mask" "F.Paste"')
        w(f'\t\t(pad "{p["num"]}" {p["kind"]} {p["shape"]} (at {p["dx"]:.4f} {p["dy"]:.4f}) (size {p["size"][0]} {p["size"][1]})'
          + (f' (drill {p["drill"]})' if p["drill"] else "") + f' (layers {L}) (remove_unused_layers no)'
          + (f' (net "{p["net"]}")' if p["net"] else "") + f' (uuid "{uid()}"))')
        if p["kind"] != "np_thru_hole":
            PADS.append(dict(ref=ref, num=p["num"], net=p["net"], x=x + p["dx"], y=y + p["dy"], w=p["size"][0], h=p["size"][1],
                             shape=p["shape"], tht=p["kind"] == "thru_hole"))
    w('\t)')


# ---------- geometrie (identique au SVG) ----------
body, _, _, ls = M.tab_geoms()
vis = []
for i, (c, g) in enumerate(ls):                         # cuivre visible : bord (ou lettre pleine) moins les lettres posees dessus
    cu = c if g is None else c.difference(g)
    later = unary_union([c2 for c2, _ in ls[i + 1:]]) if i + 1 < len(ls) else Polygon()
    vis.append(cu.difference(later))
copper = unary_union(vis).intersection(body.buffer(-EDGE_CLEAR)).buffer(0)

src = open("pcb-polygon/c3-tft-meatboy-polygon.kicad_pcb", encoding="utf-8").read()
o.append(src[:src.index("\t(footprint")].rstrip("\n"))     # en-tete + calques + reglages repris tel quel
for p in geoms(body):                                      # Edge.Cuts : exterieur + contreformes (trous des lettres)
    poly(list(p.exterior.coords)[:-1], "Edge.Cuts", fill=False, width=.15)
    for h in p.interiors:
        poly(list(h.coords)[:-1], "Edge.Cuts", fill=False, width=.15)
fill_layers(copper, ["F.Cu", "F.Mask"])                      # lettres cuivre : cuivre + ouverture de vernis

# QR : plaque cuivre ; vernis ouvert sur les modules clairs seulement (modules sombres = cuivre sous vernis)
q = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M, border=0)
q.add_data(M.URL)
q.make(fit=True)
mx = q.get_matrix()
n = len(mx)
u = M.QS / n
px0, py0, pw = M.QX - 1.3, M.QY - 1.3, M.QS + 2.6
plate = box(px0, py0, px0 + pw, py0 + pw)
poly(list(plate.exterior.coords)[:-1], "F.Cu")
light = [box(M.QX + c * u, M.QY + r * u, M.QX + (c + 1) * u, M.QY + (r + 1) * u) for r in range(n) for c in range(n) if not mx[r][c]]
frame = plate.difference(box(M.QX, M.QY, M.QX + M.QS, M.QY + M.QS))
for g in light + geoms(frame):
    poly(list(g.exterior.coords)[:-1], "F.Mask")

# ---------- composants (cablage = .ino) ----------
SYc = M.SY + M.SH / 2
hdr = ["GND", "+3V3", "SCLK", "MOSI", "RST", "DC", "CS", "+3V3"]     # GND VCC SCL SDA RES DC CS BLK
footprint("TFT_2.0in_header_1x08", "J3", M.HDR_X, SYc - 8.89,
          [P(i + 1, "thru_hole", "rect" if i == 0 else "circle", 0, i * 2.54, (1.7, 1.7), hdr[i], 1.0) for i in range(8)])
# SW1 = bouton le plus a gauche = GAUCHE (GPIO2) ; SW2 = DROITE (GPIO1) ; SW3 = SAUT (GPIO3). Pattes d'un meme cote reliees dans le bouton.
for ref, (cx, cy), net in (("SW1", M.SW1, "GAUCHE"), ("SW2", M.SW2, "DROITE")):
    footprint("SW_6x6", ref, cx, cy, [P(k + 1, "thru_hole", "circle", dx, dy, (1.8, 1.8), net if dx < 0 else "GND", 1.0)
                                       for k, (dx, dy) in enumerate([(-3.25, -2.25), (-3.25, 2.25), (3.25, -2.25), (3.25, 2.25)])])
footprint("SW_12x12", "SW3", M.SW3[0], M.SW3[1], [P(k + 1, "thru_hole", "circle", dx, dy, (2.2, 2.2), "SAUT" if dx < 0 else "GND", 1.3)
                                                  for k, (dx, dy) in enumerate([(-6.25, -2.5), (-6.25, 2.5), (6.25, -2.5), (6.25, 2.5)])])
for k, (x, y) in enumerate(M.MOUNT):
    footprint("MountingHole_3.2mm_M3", f"MTG{k + 1}", x, y, [P("", "np_thru_hole", "circle", 0, 0, (3.2, 3.2), None, 3.2)])
for k, (x, y, side, nm) in enumerate(M.PADS_L):
    footprint("WirePad_3.8mm", f"P{k + 1}", x, y, [P(1, "thru_hole", "circle", 0, 0, (3.8, 3.8), nm, 1.2)])
# ESP32-C3 SuperMini : CMS au dos, vu de DEVANT (carte retournee) -> la rangee J1 (GPIO0..5V, cote gauche du module vu de dessus)
# apparait a DROITE ; USB-C vers le bas. Coordonnees des pastilles = coordonnees finales (KiCad les stocke deja retournees).
fx, fy = 56.5, 40.0
J1 = ["IO0", "DROITE", "GAUCHE", "SAUT", "SCLK", "+3V3", "GND", None]    # GPIO0,1,2,3,4,3V3,GND,5V
J2 = ["IO21", "IO20", "RST", None, "IO8", "CS", "MOSI", "DC"]            # GPIO21,20,10,9,8,7,6,5
sm = []
for k in range(8):
    for row, (nets, dx) in enumerate(((J1, +7.62), (J2, -7.62))):
        sm.append(P(k + 1 + 8 * row, "smd", "rect", dx, -8.89 + k * 2.54, (2.4, 1.3), nets[k], None, True))
footprint("ESP32-C3_SuperMini_SMD", "U1", fx, fy, sm, layer="B.Cu")

# ---------- serigraphie ----------
for (cx, cy), t in ((M.SW1, "GAUCHE"), (M.SW2, "DROITE")):
    text(t, cx, cy + 5.5, size=1.2)
text("SAUT", M.SW3[0], M.SW3[1] + 8.3, size=1.5)
text("SCAN - MODS", M.QX + M.QS / 2, M.QY - 2.4, size=1.1)
for x, y, side, nm in M.PADS_L:
    text(nm, x + (3.1 if side == "L" else -3.1), y, size=1.2, just="left" if side == "L" else "right")

# ---------- routage ----------
static = dict(body=body, copper=copper, plate=plate, mount=M.MOUNT, module=box(47.9, 28.7, 65.1, 51.3))
tracks, vias, failed = route_tab.route(PADS, static)
for (x0, y0, x1, y1, wd, ly, nt) in tracks:
    w(f'\t(segment (start {x0:.4f} {y0 + OY:.4f}) (end {x1:.4f} {y1 + OY:.4f}) (width {wd}) (layer "{ly}") (net "{nt}") (uuid "{uid()}"))')
for (x, y, nt) in vias:
    w(f'\t(via (at {x:.4f} {y + OY:.4f}) (size 0.8) (drill 0.4) (layers "F.Cu" "B.Cu") (net "{nt}") (uuid "{uid()}"))')
w('\t(embedded_fonts no)\n)')

os.makedirs(OUT_DIR, exist_ok=True)
pcb = f"{OUT_DIR}/{NAME}.kicad_pcb"
open(pcb, "w", encoding="utf-8").write("\n".join(o))
json.dump({"board": {"design_settings": {"rules": {"min_copper_edge_clearance": round(EDGE_CLEAR - .1, 2), "min_clearance": 0.2,
                                                    "min_track_width": 0.2, "min_via_diameter": 0.6, "min_through_hole_diameter": 0.3}}},
           "meta": {"version": 3}}, open(f"{OUT_DIR}/{NAME}.kicad_pro", "w"), indent=1)
print("routage : %d pistes, %d vias, echecs : %s" % (len(tracks), len(vias), failed or "aucun"))
g = f"{OUT_DIR}/gerbers"
os.makedirs(g, exist_ok=True)
run = lambda *a: subprocess.run([KICAD_CLI, *a], check=True)
run("pcb", "export", "gerbers", "--output", g, "--layers", "F.Cu,B.Cu,F.Mask,B.Mask,F.SilkS,B.SilkS,F.Paste,B.Paste,Edge.Cuts", "--board-plot-params", pcb)
run("pcb", "export", "drill", "--output", g + "/", "--format", "excellon", "--drill-origin", "absolute", "--excellon-separate-th", pcb)
