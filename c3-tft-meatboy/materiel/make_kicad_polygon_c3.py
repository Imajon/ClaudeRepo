"""pcb-polygon : remplace le module ESP32 D1 Mini (rangees a 22,86 mm) par l'ESP32-C3 SuperMini (15,24 mm, brochage du .ino
= pcb-polygon-tab), reroute (route_tab.py), remplit les zones GND, DRC, Gerbers. Part de versions/...-v1-esp32mini.kicad_pcb.
"A" : meme PCB ajoure comme polygon-ajoure-A.svg, dans pcb-polygon-ajoure-A/.
"B" : l'ajoure A + traits deco 2 mm en biais sur la partie haute, en cuivre nu (F.Cu sans vernis), dans pcb-polygon-ajoure-B/.
"B" et "C" : languette d'accroche detachable (3 attaches de 2 mm). "C" = le B + stries partout ou elles tiennent (GND, cuivre nu),
      pastille GND a cote de chaque pastille de fil, dans pcb-polygon-ajoure-C/.
"D" : le B sans languette, angles du contour arrondis R2,5 (carte du cadre detachable) + pastilles GND a cote des pastilles de fil + lecteur d'identifiant de figurine
      au dos (2 plages de contact IO0 / GND entraxe 8 mm, pull-up 10k vers 3V3 sur IO0 = ADC1_CH0), dans pcb-polygon-ajoure-D/.
"kraken" : meme console (memes contraintes que l'ajoure A, sans ajours) au contour de Kraken.svg, echelle 2,2, dans pcb-kraken/.
"manette" : la carte D au contour de la manette (make_manette_svg.py, silhouette pcb-polygon-manette/manette-silhouette.png),
      ESP32-C3 descendu pour amener l'USB-C au creux des poignees, dans pcb-polygon-manette/.
"megadrive" : idem au contour de la manette Mega Drive (pcb-polygon-megadrive/manette-silhouette.png).
"pirateN" : idem au contour du dessin pirate N (make_silhouette_svg.py) ; ses parties blanches internes (interieur.png)
      en cuivre nu GND (zone F.Cu + F.Mask ouvert), sans ajours dessous, coupees par les pistes avant.
"D-pirateN" : le D (stries, ajours) + 1 ou 2 pictos du dessin pirate N (encre de pcb-polygon-pirateN/interieur.png, yeux en creux)
      en cuivre nu GND, contournes par les pistes. "D-pirateN-grand" : un seul picto de 62 mm centre sur l'ecran, qui deborde
      tout autour jusqu'au bord ; "D-pirateN-geant" : idem en 82 mm, sans stries, coupe par les pistes (pas contourne : il bloquerait tout le routage), places au plus grand hors ecran, trous et pastilles, dans pcb-polygon-ajoure-D-pirateN/.
"D-trous" : le D avec un trou metallise de 1 mm (comme l'ecran) dans chaque pastille des boutons et de l'ESP32-C3
      (trou au pas du module, rangees a 15,24 mm), dans pcb-polygon-ajoure-D-trous/.
Usage : python make_kicad_polygon_c3.py [A|B|C|D|D-trous|D-pirateN|manette|megadrive|pirateN|kraken]"""
import math, re, subprocess, uuid, zipfile, glob, os, sys
from shapely.geometry import Polygon, Point, box, LineString
from shapely.ops import unary_union
import route_tab

V = sys.argv[1] if len(sys.argv) > 1 else ""


def pictos(folder, free, n=4, hmin=9, center=None, height=0):
    """parties blanches du dessin (interieur.png) mises a l'echelle et posees dans free : n pictos, du plus grand au plus petit"""
    import numpy as np, contourpy
    from PIL import Image, ImageFilter
    from shapely import affinity
    from shapely.prepared import prep
    def poly(png, inv):
        im = Image.open(f"{folder}/{png}").convert("L")
        a = np.asarray(im.resize((im.width * 2, im.height * 2), Image.LANCZOS).filter(ImageFilter.GaussianBlur(2)), float) / 255
        g = None
        for q in sorted((Polygon(l).buffer(0) for l in contourpy.contour_generator(z=np.pad(1 - a if inv else a, 2), line_type="Separate").lines(.5)
                         if len(l) > 3), key=lambda q: -q.area):
            g = q if g is None else g.symmetric_difference(q)
        return g
    g = poly("manette-silhouette.png", 1)
    sil = max(getattr(g, "geoms", [g]), key=lambda q: q.area)
    sil = Polygon(sil.exterior)
    white = sil.difference(poly("interieur.png", 1)).buffer(-3).buffer(3)
    white = sil.difference(white.buffer(-15)).buffer(-10).buffer(10)     # picto = encre (crane dore, yeux verts) ; ~50 px/mm a 20 mm : traits +0,6 mm
    x0, y0, x1, y1 = sil.bounds
    if center:      # grand : hauteur imposee, centre impose
        k = height / (y1 - y0)
        return [affinity.affine_transform(white.simplify(1), [k, 0, 0, k, center[0] - (x0 + x1) / 2 * k, center[1] - (y0 + y1) / 2 * k])]
    out = []
    for _ in range(n):
        pf = prep(free)
        for h in range(34, hmin - 1, -1):
            k = h / (y1 - y0)
            s0 = affinity.affine_transform(sil.simplify(2), [k, 0, 0, k, -x0 * k, -y0 * k])
            w = (x1 - x0) * k
            hit = next(((x, y) for y in range(0, 80) for x in range(0, 116) if pf.contains(affinity.translate(s0, x, y))), None)
            if hit:
                break
        else:
            break
        print(f"picto {h} mm en {hit}")
        out.append(affinity.affine_transform(white.simplify(1), [k, 0, 0, k, hit[0] - x0 * k, hit[1] - y0 * k]))
        free = free.difference(affinity.translate(s0, *hit).buffer(2))
    return out
SRC = "pcb-polygon/versions/c3-tft-meatboy-polygon-v1-esp32mini.kicad_pcb"
KRAKEN = V == "kraken"
NAME = "c3-tft-meatboy-" + ("kraken" if KRAKEN else "polygon" + (f"-ajoure-{V}" if V else ""))
MAN = V in ("manette", "megadrive") or V.startswith("pirate")
if MAN:                                                                   # meme carte que D, autre contour
    NAME, V = f"c3-tft-meatboy-polygon-{V}", "D"
    os.environ["MANETTE"] = NAME[len("c3-tft-meatboy-polygon-"):]
PICTO = V[2:] if V.startswith("D-pirate") else ""                        # le D + picto(s) pirate en cuivre nu
GRAND = {"-grand": 62, "-geant": 82}.get(PICTO[PICTO.rfind("-"):], 0) if "-" in PICTO else 0   # un seul picto (hauteur mm) centre sur l'ecran
PICTO = PICTO.split("-")[0]
if PICTO:
    V = "D"
TROUS = V == "D-trous"                                                    # le D + trous dans les pastilles boutons / ESP
if TROUS:
    V = "D"
D = "pcb-" + NAME[len("c3-tft-meatboy-"):]
PCB = f"{D}/{NAME}.kicad_pcb"
KICAD_CLI = r"C:\Users\info\AppData\Local\Programs\KiCad\10.0\bin\kicad-cli.exe"
uid = lambda: str(uuid.uuid4())
RENAME = {"GPIO32_DROITE": "DROITE", "GPIO33_GAUCHE": "GAUCHE", "GPIO4_SAUT": "SAUT", "GPIO18_SCLK": "SCLK",
          "GPIO23_MOSI": "MOSI", "GPIO25_RST": "RST", "GPIO26_DC": "DC", "GPIO27_CS": "CS"}

s = open(SRC, encoding="utf-8").read()
for a, b in RENAME.items():
    s = s.replace(f'"{a}"', f'"{b}"')
s = re.sub(r'(\n\t\t\(pad "8" thru_hole circle\n\t\t\t\(at 0 17\.78\)\n(?:\t\t\t[^\n]*\n)*?\t\t\t\(remove_unused_layers no\))',   # J3 broche 8 = BL de l'ecran
           r'\1\n\t\t\t(net "+3V3")', s, count=1)                                                                   # -> retroeclairage toujours allume
assert s.count('(net "+3V3")') >= 2, "BL de J3 non relie"
blocks = re.findall(r"\n\t\(.*?\n\t\)(?=\n)", s, re.S)              # elements de premier niveau multi-lignes
fx, fy = 59.43, 48.89                                                 # centre de l'ancien module (J1 x=48, J2 x=70.86, y 40..57.78)
SX, SY = 25.5, 10.76                                                  # coin de l'ecran (pcb-polygon)
ART0 = Polygon()                                                      # pirate : dessin en cuivre
if V:       # module au bord du bas, cote USB vers le bas ; le contour est coupe a plat (largeur 2*CUT) sous l'USB-C
    fx, USB_Y, CUT = (69.6, 101.0, 6.5) if KRAKEN else (56.42, 73.0, 12)
    if MAN:
        import make_manette_svg as MS
        USB_Y += MS.ESP_DY
        ART0 = MS.COPPER if MS.COPPER is not None else Polygon()            # pirate : parties blanches du dessin
    fy = USB_Y - 11.25
M_KEEP = []                                                            # encombrement des boutons deplaces (pas d'ajour dessous)
if KRAKEN:  # implantation verifiee contre le contour (boutons sous les bras, de part et d'autre de l'ecran)
    SX, SY = 34.3, 38.0                # connecteur J3 (SX+60) juste avant le creux tete/bras droit (x=96.3)
    MOVE = {"J3": (SX + 60.05, SY + 37.1 / 2 - 8.89, 0), "SW1": (19 - 2.25, 61.5 + 4.25, 90), "SW2": (29.5 - 2.25, 61.5 + 4.25, 90),
            "SW3": (113.5 - 7.25, 61.5 - 2.5, 0), "MTG1": (52, 30, 0), "MTG2": (87.7, 30, 0), "MTG3": (40, 104, 0), "MTG4": (99.7, 104, 0)}
elif MAN:   # megadrive : boutons sortis du creux par make_manette_svg.py
    MOVE = {ref: (x, y, 0) for ref, (x, y) in MS.MOVE.items()}
    M_KEEP = [box(x + MS.BODY[r][0] - MS.BODY[r][2] / 2 - 1, y + MS.BODY[r][1] - MS.BODY[r][2] / 2 - 1,
                  x + MS.BODY[r][0] + MS.BODY[r][2] / 2 + 1, y + MS.BODY[r][1] + MS.BODY[r][2] / 2 + 1) for r, (x, y, _) in MOVE.items() if r in MS.BODY]
if KRAKEN or MAN:
    for b in re.findall(r"\n\t\(footprint.*?\n\t\)(?=\n)", s, re.S):
        ref = re.search(r'"Reference" "([^"]*)"', b).group(1)
        if ref in MOVE:
            x, y, a = MOVE[ref]
            s = s.replace(b, re.sub(r"\n\t\t\(at [^)]*\)", f"\n\t\t(at {x:.4f} {y:.4f}{f' {a}' if a else ''})", b, count=1))
    s = re.sub(r"(\n\t\t\(polygon\n\t\t\t\(pts).*?(\n\t\t\t\))", r"\1 (xy 0 0) (xy 141 0) (xy 141 118) (xy 0 118)\2", s, flags=re.S)   # zones GND


def drop(b):
    return (b.startswith("\n\t(segment") or b.startswith("\n\t(via") or b.startswith("\n\t(gr_rect") and '"B.Fab"' in b
            or b.startswith("\n\t(footprint") and re.search(r'"Reference" "J[12]"', b))


for b in blocks:
    if drop(b):
        s = s.replace(b, "", 1)
s = re.sub(r"\n\t\t\(filled_polygon.*?\n\t\t\)", "", s, flags=re.S)  # zones remplies a nouveau par kicad-cli

if TROUS:   # pastilles CMS des boutons -> traversantes, trou de 1 mm comme le connecteur de l'ecran (J3)
    for b in re.findall(r"\n\t\(footprint.*?\n\t\)(?=\n)", s, re.S):
        if re.search(r'"Reference" "SW\d"', b):
            nb = b.replace(" smd roundrect", " thru_hole rect").replace('(layers "F.Cu" "F.Mask" "F.Paste")', '(drill 1)\n\t\t\t(layers "*.Cu" "*.Mask")')
            assert nb.count("(drill 1)") == 4, "pastilles des boutons non percees"
            nb = nb.replace("(duplicate_pad_numbers_are_jumpers no)", "(duplicate_pad_numbers_are_jumpers yes)")   # pattes de meme numero reliees dans le bouton (SW2 : un GND enferme par MOSI/CS)
            s = s.replace(b, nb)

# ---------- pastilles existantes (coordonnees absolues) ----------
PADS, MOUNT = [], []
for b in re.findall(r"\n\t\(footprint.*?\n\t\)(?=\n)", s, re.S):
    ref = re.search(r'"Reference" "([^"]*)"', b).group(1)
    X, Y, *A = map(float, re.search(r"\n\t\t\(at ([^)]*)\)", b).group(1).split())
    t = math.radians(A[0] if A else 0)
    for p in re.findall(r"\n\t\t\(pad .*?\n\t\t\)", b, re.S):
        num, kind, shape = re.match(r'\n\t\t\(pad "([^"]*)" (\S+) (\S+)', p).groups()
        x, y, *pa = map(float, re.search(r"\(at ([^)]*)\)", p).group(1).split())
        w, h = map(float, re.search(r"\(size ([^)]*)\)", p).group(1).split())
        if pa and round(pa[0]) % 180 == 90:
            w, h = h, w
        ax, ay = X + x * math.cos(t) + y * math.sin(t), Y - x * math.sin(t) + y * math.cos(t)
        if kind == "np_thru_hole":
            MOUNT.append((ax, ay))
            continue
        net = re.search(r'\(net "([^"]*)"\)', p)
        PADS.append(dict(ref=ref, num=num, net=net and net.group(1), x=ax, y=ay, w=w, h=h,
                         shape="circle" if shape == "circle" else "rect", tht=kind == "thru_hole", front='"F.Cu"' in p))

# ---------- ESP32-C3 SuperMini au dos (identique a make_kicad_tab.py) ----------
J1 = [None, "DROITE", "GAUCHE", "SAUT", "SCLK", "+3V3", "GND", None]      # GPIO0,1,2,3,4,3V3,GND,5V
J2 = [None, None, "RST", None, None, "CS", "MOSI", "DC"]                  # GPIO21,20,10,9,8,7,6,5
if V:                                                                     # pastilles de fil du dessin ajoure
    J1[0], J2[0], J2[1], J2[4] = "IO0", "IO21", "IO20", "IO8"
o = [f'\t(footprint "ESP32-C3_SuperMini_SMD" (layer "B.Cu") (uuid "{uid()}") (at {fx:.4f} {fy:.4f})',
     f'\t\t(property "Reference" "U1" (at 0 0) (layer "B.SilkS") (hide yes) (uuid "{uid()}") (effects (font (size 1 1) (thickness .15)) (justify mirror)))',
     f'\t\t(property "Value" "ESP32-C3_SuperMini_SMD" (at 0 0) (layer "B.Fab") (hide yes) (uuid "{uid()}") (effects (font (size 1 1) (thickness .15)) (justify mirror)))',
     f'\t\t(fp_rect (start -9 -11.25) (end 9 11.25) (stroke (width .1) (type default)) (fill no) (layer "B.Fab") (uuid "{uid()}"))']
PW = {"C": 3.4, "D": 4.4}.get(V, 2.4)                                    # C : +1 mm, D : +2 mm vers l'exterieur (soudure a la main, depasse du module)
for k in range(8):
    for row, (nets, dx) in enumerate(((J1, +7.62), (J2, -7.62))):
        dy, nt = -8.89 + k * 2.54, nets[k]
        pw = 2.4 if k == 7 else PW                                            # rangee du bas : trop pres du bord (coupe USB-C)
        dx += (pw - 2.4) / 2 * (1 if dx > 0 else -1)                          # bord interieur inchange
        hx = (7.62 if dx > 0 else -7.62) if TROUS else dx                     # KiCad : (at) = trou (au pas du module), cuivre decale par offset
        lay = f'(drill 1 (offset {dx - hx:.4f} 0)) (layers "*.Cu" "*.Mask")' if TROUS else '(layers "B.Cu" "B.Mask" "B.Paste")'
        o.append(f'\t\t(pad "{k + 1 + 8 * row}" {"thru_hole" if TROUS else "smd"} rect (at {hx:.4f} {dy:.4f}) (size {pw} 1.3) {lay}'
                 + (f' (net "{nt}")' if nt else "") + f' (uuid "{uid()}"))')
        PADS.append(dict(ref="U1", num=k + 1 + 8 * row, net=nt, x=fx + dx, y=fy + dy, w=pw, h=1.3, shape="rect", tht=TROUS, hx=fx + hx))
o.append("\t)")

# ---------- routage ----------
edges = [tuple(map(float, re.search(r"\(start ([^)]*)\)", b).group(1).split())) + tuple(map(float, re.search(r"\(end ([^)]*)\)", b).group(1).split()))
         for b in re.findall(r"\n\t\(gr_line.*?\n\t\)", s, re.S) if '"Edge.Cuts"' in b]
ring, rest = [edges[0][:2], edges[0][2:]], edges[1:]
while rest:
    e = min(rest, key=lambda e: min(math.dist(ring[-1], e[:2]), math.dist(ring[-1], e[2:])))
    rest.remove(e)
    ring.append(e[2:] if math.dist(ring[-1], e[:2]) < math.dist(ring[-1], e[2:]) else e[:2])
body = Polygon(ring).buffer(0)


def kraken_body():
    """Kraken.svg (un seul chemin, contreformes en pair/impair), echelle 2,2, coin haut-gauche en (1.5, 1.5)."""
    from svgelements import SVG, Path
    import numpy as np
    from shapely import affinity
    e = next(e for e in SVG.parse("Kraken.svg").elements() if isinstance(e, Path))
    g = None
    for p in sorted((Polygon(Path(sp).npoint(np.linspace(0, 1, 40 * max(1, len(Path(sp))) + 1)) * 25.4 / 96).buffer(0)
                     for sp in e.as_subpaths()), key=lambda p: -p.area):
        g = p if g is None else g.symmetric_difference(p)
    x0, y0 = g.bounds[:2]
    return affinity.affine_transform(g, [2.2, 0, 0, 2.2, 1.5 - 2.2 * x0, 1.5 - 2.2 * y0]).simplify(.01)


if KRAKEN:
    body = kraken_body()
if V:       # contour coupe a plat sous le module : la prise USB-C arrive au bord de la carte
    body = body.difference(box(fx - CUT, USB_Y, fx + CUT, 130))
    if MAN:         # contour de la manette (bord plat de 24 mm sous l'USB-C) ; zones GND = la carte
        body = MS.shape.difference(box(fx - CUT, USB_Y, fx + CUT, 200)).buffer(-.5, join_style=1).buffer(.5, join_style=1).simplify(.01)   # sans pointe aux coins de la coupe USB
        ring = "(xy -40 -60) (xy 250 -60) (xy 250 200) (xy -40 200)"                 # zones GND : grand rectangle, coupe par le contour
        s = re.sub(r"(\n\t\t\(polygon\n\t\t\t\(pts).*?(\n\t\t\t\))", lambda m: m.group(1) + " " + ring + m.group(2), s, flags=re.S)
    elif V == "D":  # angles du contour arrondis R2,5 (pointes et creux)
        body = body.buffer(2.5, join_style=1).buffer(-5, join_style=1).buffer(2.5, join_style=1).simplify(.01)
    for b in re.findall(r"\n\t\(gr_line.*?\n\t\)", s, re.S):
        if '"Edge.Cuts"' in b:
            s = s.replace(b, "", 1)
    outline = body
    if V in ("B", "C"):   # languette d'accroche (facon Pocket Operator) : oblong 66 x 10,75 mm + trou euro, 3 attaches de 2 mm sur les pointes
        from shapely import affinity
        PEAKS, TIP, SLOPE, NW = [29.06, 56.69, 84.29], 1.5, (9.11 - 1.5) / (56.69 - 42.87), 2.0
        BRK = TIP + NW / 2 * SLOPE                                   # cassure la ou la pointe fait 2 mm (pointe tronquee de 0,55 mm)
        TW, TOP, TBOT, TX = 66.0, -9.75, 1.0, 56.69
        obround = lambda x, y, w, h: box(x - (w - h) / 2, y, x + (w - h) / 2, y).buffer(h / 2, quad_segs=32)
        ob = obround(TX, (TOP + TBOT) / 2, TW, TBOT - TOP)
        slot = obround(TX, (TOP + TBOT) / 2, 26.0, 4.0).union(                       # trou 26 x 4 mm + demi-cercle R3,5 vers le haut
            Point(TX, (TOP + TBOT) / 2).buffer(3.5, quad_segs=32).intersection(box(TX - 3.5, TOP, TX + 3.5, (TOP + TBOT) / 2)))
        notches = unary_union([Point(x + sx * NW / 2, BRK).buffer(.35, quad_segs=8) for x in PEAKS for sx in (-1, 1)])   # amorces
        body = body.difference(box(0, -50, 140, BRK))
        TABG = ob.union(unary_union([box(x - NW / 2, TBOT - 3, x + NW / 2, BRK) for x in PEAKS])).difference(slot).difference(notches)
        outline = body.union(TABG).difference(notches)
        rim = ob.buffer(-.55).difference(ob.buffer(-1.55)).difference(slot.buffer(.6))   # bord cuivre nu de 1 mm, a 0,55 mm du bord (contourne le demi-cercle)
        cy = (TOP + TBOT) / 2                                                       # anneau coupe en 2 (gr_poly sans trou)
        TABCU = [rim.intersection(box(0, -50, 140, cy)), rim.intersection(box(0, cy, 140, 50))]
        zt = affinity.rotate(ob.buffer(-2.15).difference(slot.buffer(.8)), 45, origin=(0, 0))   # stries de la carte prolongees
        for k in range(-60, 60):
            bd = zt.intersection(box(-300, k * 4.0, 300, k * 4.0 + 2.0)).buffer(-.3).buffer(.3)
            TABCU += [affinity.rotate(r, -45, origin=(0, 0)) for r in getattr(bd, "geoms", [bd]) if r.geom_type == "Polygon" and r.area > 3]
        ring = " ".join(f"(xy {x:.4f} {y:.4f})" for x, y in list(body.exterior.coords)[:-1])   # zones GND : la carte seule
        s = re.sub(r"(\n\t\t\(polygon\n\t\t\t\(pts).*?(\n\t\t\t\))", lambda m: m.group(1) + " " + ring + m.group(2), s, flags=re.S)
    for r in [outline.exterior, *outline.interiors]:
        o.append(f'\t(gr_poly (pts {" ".join(f"(xy {x:.4f} {y:.4f})" for x, y in list(r.coords)[:-1])}) '
                 f'(stroke (width .15) (type default)) (fill no) (layer "Edge.Cuts") (uuid "{uid()}"))')
plate = Polygon()
TABG, TABCU = (TABG, TABCU) if V in ("B", "C") else (Polygon(), [])
if V:       # ajours + QR cuivre + 4 pastilles de fil (dessin polygon-ajoure-<V>.svg)
    import make_polygon_ajoure as M, qrcode
    SILK = []

    def sk(t, x, y, sz=1.0, side="F"):
        SILK.append((t, x, y, sz, side))
        o.append(f'\t(gr_text "{t}" (at {x:.3f} {y:.3f}) (layer "{side}.SilkS") (uuid "{uid()}") (effects (font (size {sz} {sz}) (thickness {sz * .15:.3f}))'
                 + (" (justify mirror)" if side == "B" else "") + "))")
    WIRE = [(8.5, 46.5, "L", "IO21"), (12.0, 52.3, "L", "IO20"), (131.2, 46.5, "R", "IO0"), (127.7, 52.3, "R", "IO8")] if KRAKEN else M.PADS_L
    LED_POS = [(57, 9.8, 12.5), (69.85, 5.8, 8.5), (82.7, 9.8, 12.5)] if KRAKEN \
        else [(29.06, 4.6, 7.3), (56.69, 4.6, 7.3), (84.29, 4.6, 7.3)]                  # (x, y resistance, y LED)
    if V in ("C", "D"):    # pastille GND a cote de chaque pastille de fil (cote interieur, 1,2 mm d'ecart)
        WIRE = WIRE + [(x + (5.0 if side == "L" else -5.0), y, side, "GND") for x, y, side, _ in WIRE]
    for k, (x, y, side, nm) in enumerate(WIRE):
        o.append(f'\t(footprint "WirePad_3.8mm" (layer "F.Cu") (uuid "{uid()}") (at {x:.4f} {y:.4f})\n'
                 f'\t\t(property "Reference" "P{k + 1}" (at 0 0) (layer "F.SilkS") (hide yes) (uuid "{uid()}") (effects (font (size 1 1) (thickness .15))))\n'
                 f'\t\t(pad "1" thru_hole circle (at 0 0) (size 3.8 3.8) (drill 1.2) (layers "*.Cu" "*.Mask") (net "{nm}") (uuid "{uid()}"))\n\t)')
        PADS.append(dict(ref=f"P{k + 1}", num=1, net=nm, x=x, y=y, w=3.8, h=3.8, shape="circle", tht=True))
        sk(nm, x + (.5 if side == "L" else -.5), y + (-3.0 if k % 2 == 0 else 3.0))      # etiquette au-dessus / en dessous
    # LED optionnelles (en haut, une par pointe) : +3V3 -> R -> LED -> fil du bouton. Bouton appuye = fil a GND = LED allumee.
    # Sans LED ni R soudees, rien ne change (le .ino lit les boutons en INPUT_PULLUP). 1206 pour souder a la main.
    LEDS = []
    for k, ((xt, yr, yd), nm) in enumerate(zip(LED_POS, ("GAUCHE", "DROITE", "SAUT"))):
        for ref, y, nets in ((f"R{k + 1}", yr, ("+3V3", f"LED_{nm}")), (f"D{k + 1}", yd, (nm, f"LED_{nm}"))):   # D : pad 1 = cathode
            pads = ((-1.4625, nets[0]), (1.4625, nets[1]))
            o.append(f'\t(footprint "{"LED" if ref[0] == "D" else "R"}_1206_3216Metric" (layer "F.Cu") (uuid "{uid()}") (at {xt:.4f} {y:.4f})\n'
                     f'\t\t(property "Reference" "{ref}" (at 0 0) (layer "F.SilkS") (hide yes) (uuid "{uid()}") (effects (font (size 1 1) (thickness .15))))\n'
                     + "".join(f'\t\t(pad "{i + 1}" smd roundrect (at {dx} 0) (size 1.125 1.75) (layers "F.Cu" "F.Paste" "F.Mask") (roundrect_rratio .2) (net "{nt}") (uuid "{uid()}"))\n'
                               for i, (dx, nt) in enumerate(pads))
                     + (f'\t\t(fp_line (start -2.3 -1.1) (end -2.3 1.1) (stroke (width .2) (type default)) (layer "F.SilkS") (uuid "{uid()}"))\n' if ref[0] == "D" else "")
                     + "\t)")
            for i, (dx, nt) in enumerate(pads):
                PADS.append(dict(ref=ref, num=i + 1, net=nt, x=xt + dx, y=y, w=1.125, h=1.75, shape="rect", tht=False, front=True))
        LEDS.append((xt, yr, yd))
    # QR au dos (derriere l'ecran / la tete du Kraken) ; colonnes en miroir pour se lire de derriere
    QX, QY, QS = (62.85, 12.0, M.QS) if KRAKEN else (43.0, 18.0, M.QS)
    URL = M.URL
    if V in ("B", "C", "D"):   # QR au maximum : plaque de y = 10 (creux du bord haut a 9,1) a y = 48 (module a 50,5), centree sur le module
        QS = 35.4
        QX, QY = fx - QS / 2, 11.3
        QS = min(QS, fy - 11.25 - 2 - 1.3 - QY)                                    # manette : ESP remonte -> QR reduit pour garder 2 mm
        QX = fx - QS / 2
    q = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M, border=0)
    q.add_data(URL)
    q.make(fit=True)
    QM = q.get_matrix()
    n, u = len(QM), QS / len(QM)
    plate = box(QX - 1.3, QY - 1.3, QX + QS + 1.3, QY + QS + 1.3)
    gp = lambda g, ly, fill="yes": o.append(f'\t(gr_poly (pts {" ".join(f"(xy {x:.4f} {y:.4f})" for x, y in list(g.exterior.coords)[:-1])}) '
                                            f'(stroke (width {.15 if ly == "Edge.Cuts" else .1}) (type default)) (fill {fill}) (layer "{ly}") (uuid "{uid()}"))')
    gp(plate, "B.Cu")
    x0, y0, x1, y1 = plate.bounds                                          # vernis ouvert : modules clairs + cadre (4 bandes)
    for g in [box(QX + (n - 1 - c) * u, QY + r * u, QX + (n - c) * u, QY + (r + 1) * u) for r in range(n) for c in range(n) if not QM[r][c]] \
            + [box(x0, y0, x1, QY), box(x0, QY + QS, x1, y1), box(x0, QY, QX, QY + QS), box(QX + QS, QY, x1, QY + QS)]:
        gp(g, "B.Mask")
    if V not in ("B", "C", "D"):                                                 # B, C, D : plus de place pour l'etiquette
        sk("SCAN - MODS", QX + QS / 2, QY - 2.4, 1.1, "B")
    if V == "D":    # lecteur d'identifiant des figurines (dos) : plage IO0 + plage GND (4 x 5 mm, entraxe 8 mm), pull-up 10k 1206 en dessous
        busy = unary_union([plate.buffer(2), box(fx - 10.5, fy - 12.5, fx + 10.5, fy + 12.5), *[Point(m).buffer(4.5) for m in MOUNT],
                            *[route_tab.padgeom(p).buffer(1.5) for p in PADS], *[Point(x, y).buffer(4) for x, y, *_ in WIRE]])
        ok = body.buffer(-2.0)
        IDX, IDY = min(((x / 2, y / 2) for x in range(20, 230) for y in range(20, 150)
                        if ok.contains(box(x / 2 - 6, y / 2 - 2.5, x / 2 + 6, y / 2 + 5.5)) and not busy.intersects(box(x / 2 - 6, y / 2 - 2.5, x / 2 + 6, y / 2 + 5.5))),
                       key=lambda q: math.dist(q, (90, 55)))
        for ref, pads in (("JID", ((-4.0, 0, 4.0, 5.0, "IO0", '"B.Cu" "B.Mask"'), (4.0, 0, 4.0, 5.0, "GND", '"B.Cu" "B.Mask"'))),
                          ("RID", ((-1.4625, 4.2, 1.125, 1.75, "+3V3", '"B.Cu" "B.Mask" "B.Paste"'), (1.4625, 4.2, 1.125, 1.75, "IO0", '"B.Cu" "B.Mask" "B.Paste"')))):
            o.append(f'\t(footprint "{"ID_Contacts" if ref == "JID" else "R_1206_3216Metric"}" (layer "F.Cu") (uuid "{uid()}") (at {IDX:.4f} {IDY:.4f})\n'
                     f'\t\t(property "Reference" "{ref}" (at 0 0) (layer "F.Fab") (hide yes) (uuid "{uid()}") (effects (font (size 1 1) (thickness .15))))\n'
                     + "".join(f'\t\t(pad "{i + 1}" smd roundrect (at {dx} {dy}) (size {w} {h}) (layers {ly}) (roundrect_rratio .15) (net "{nt}") (uuid "{uid()}"))\n'
                               for i, (dx, dy, w, h, nt, ly) in enumerate(pads)) + "\t)")
            for i, (dx, dy, w, h, nt, ly) in enumerate(pads):
                PADS.append(dict(ref=ref, num=i + 1, net=nt, x=IDX + dx, y=IDY + dy, w=w, h=h, shape="rect", tht=False))
        sk("ID", IDX, IDY - 3.4, .9, "B")
        sk("10k", IDX + 3.8, IDY + 4.2, .8, "B")
        print(f"lecteur ID au dos : ({IDX}, {IDY})")
    if GRAND:
        ART0 = unary_union(pictos(f"pcb-polygon-{PICTO}", None, center=(SX + M.SW / 2, SY + M.SH / 2 + 4), height=GRAND))
        print(f"grand picto : {ART0.bounds}")
    elif PICTO:   # pictos : le plus grand qui tienne hors ecran/trous/pastilles, puis un 2e a cote
        ART0 = unary_union(pictos(f"pcb-polygon-{PICTO}", body.buffer(-1.5).difference(unary_union(
            [box(SX - 1, SY - 1, SX + M.SW + 1, SY + M.SH + 1), *[Point(m).buffer(4) for m in MOUNT], box(fx - 12, fy - 14, fx + 12, fy + 14),
             *[route_tab.padgeom(p).buffer(1) for p in PADS if p["tht"] or p.get("front")],
             *[box(x - 3.5, y - 4.4, x + 3.5, y + 4.4) for x, y, *_ in WIRE]]))))
    M.FREE = body.buffer(-2.4).difference(unary_union([
        M.KEEP, *M_KEEP, ART0.buffer(1.0), plate.buffer(1.5), box(fx - 10.5, fy - 12.5, fx + 10.5, fy + 12.5),
        *[Point(m).buffer(4.5) for m in MOUNT], *[Point(p["x"], p["y"]).buffer(max(p["w"], p["h"]) / 2 + 2) for p in PADS],
        *[box(x - 2.5, y - 4.4, x + 3.5, y + 4.4) if side == "L" else box(x - 3.5, y - 4.4, x + 2.5, y + 4.4) for x, y, side, _ in WIRE]]))
    H = [] if KRAKEN else M.holes("A" if V in ("B", "C", "D") else V)                                       # Kraken : pas d'ajours en plus du dessin
    body = body.difference(unary_union(H))
    for h in H:
        gp(h, "Edge.Cuts", "no")
    print(len(H), "ajours")
static = dict(body=body, copper=ART0.buffer(.3) if PICTO and not GRAND else Polygon(), plate=Polygon(), mount=MOUNT,      # QR au dos : cuivre fixe cote B (avec garde)
              module=box(fx - 8.6, fy - 11.3, fx + 8.6, fy + 11.3), back_cu=plate)
tracks, vias, failed = route_tab.route(PADS, static, ("GND",) if V else (),   # ajoure : GND par les zones
                                      ("+3V3", "RST", "CS", "IO8") if KRAKEN else ())          # Kraken : +3V3 (jusqu'au BL de J3) d'abord, sinon RST/CS/IO8 bloques ou ilot GND
STRIPES, ARTP = [], []


def nohole(g):
    """gr_poly sans trou : coupe en 2 par une verticale passant par le 1er trou, jusqu'a ce qu'il n'y en ait plus"""
    if not g.interiors:
        return [g]
    c, (x0, y0, x1, y1) = g.interiors[0].centroid.x, g.bounds
    return [r for h in (box(x0 - 1, y0 - 1, c, y1 + 1), box(c, y0 - 1, x1 + 1, y1 + 1))
            for r in getattr(g.intersection(h), "geoms", [g.intersection(h)]) if r.geom_type == "Polygon" for r in nohole(r)]


if not ART0.is_empty:   # dessin en cuivre nu (aussi sous l'ecran), hors trous, pastilles, pistes avant, vias, serigraphie
    CLR = .45
    art = ART0.intersection(body.buffer(-1.0)).difference(unary_union(
        [*[Point(m).buffer(3.2) for m in MOUNT],
         *[LineString([(a, b), (c, d)]).buffer(wd / 2 + CLR) for a, b, c, d, wd, ly, _ in tracks if ly == "F.Cu"],
         *[Point(x, y).buffer(.4 + CLR) for x, y, _ in vias],
         *[route_tab.padgeom(p).buffer(1.6 if p["tht"] else CLR + .15) for p in PADS if p["tht"] or p.get("front")],   # THT : place pour les rayons GND
         *[box(x - len(t) * sz * .5 - .5, y - sz - .5, x + len(t) * sz * .5 + .5, y + sz + .5) for t, x, y, sz, sd in SILK if sd == "F"],
         *[box(xt - 2.8, yd - 1.6, xt - 1.8, yd + 1.6) for xt, _, yd in LEDS],
         *[box(x - 3, y - 5.4, x + 3, y - 2.9) for x, y in MOUNT]])).buffer(-.3).buffer(.3)     # pas de languettes < 0,6 mm
    ARTP = [q.simplify(.01) for r in getattr(art, "geoms", [art]) if r.geom_type == "Polygon" and r.area > 3 for q in nohole(r)]
    for k, g in enumerate(ARTP):      # vernis ouvert + zone GND : le dessin fait partie du plan de masse (relie les ilots au lieu de les couper)
        pp = " ".join(f"(xy {x:.4f} {y:.4f})" for x, y in list(g.exterior.coords)[:-1])
        o.append(f'	(gr_poly (pts {pp}) (stroke (width 0) (type solid)) (fill yes) (layer "F.Mask") (uuid "{uid()}"))')
        o.append(f'	(zone (net "GND") (layer "F.Cu") (uuid "{uid()}") (hatch none 0.5) (priority {k + 1}) (connect_pads yes (clearance 0.5))'
                 f' (min_thickness 0.2) (fill yes (thermal_gap 0.5) (thermal_bridge_width 0.5) (island_removal_mode 1)) (polygon (pts {pp})))')   # priorite propre : morceaux voisins (nohole) sans conflit
    print(f"dessin cuivre : {len(ARTP)} morceaux, {sum(g.area for g in ARTP):.0f} mm2")
if V in ("B", "C", "D") and not MAN and GRAND < 80:   # geant : plus de stries   # (pirate : pas de stries, le dessin seul) traits deco : bandes 2 mm a 45 deg (pas 4 mm), cuivre sans vernis, hors pistes/pastilles ; B : haut de la carte, sans net
    from shapely import affinity                                    # C : partout ou ils tiennent, en GND (relient les ilots du plan de masse)
    W, PITCH, YMAX, CLR = (2.0, 4.0, 31.5, .45) if V in ("B", "D") else (2.0, 4.0, 200, .55)
    zone = body.buffer(-1.0).intersection(box(0, 0, 200, YMAX)).difference(unary_union(
        [box(SX - .5, SY - .5, SX + M.SW + .5, SY + M.SH + .5), *[Point(m).buffer(3.2) for m in MOUNT],
         *[LineString([(a, b), (c, d)]).buffer(wd / 2 + CLR) for a, b, c, d, wd, ly, _ in tracks if ly == "F.Cu"],
         *[Point(x, y).buffer(.4 + CLR) for x, y, _ in vias],
         *[route_tab.padgeom(p).buffer(CLR) for p in PADS if p["tht"] or p.get("front")],
         *[box(x - len(t) * sz * .5 - .5, y - sz - .5, x + len(t) * sz * .5 + .5, y + sz + .5) for t, x, y, sz, sd in SILK if sd == "F"],
         *[box(xt - 2.8, yd - 1.6, xt - 1.8, yd + 1.6) for xt, _, yd in LEDS],                 # trait cathode des LED
         *[box(x - 3, y - 5.4, x + 3, y - 2.9) for x, y in MOUNT], ART0.buffer(.6)]))         # reference MTG ; dessin pirate
    rz = affinity.rotate(zone, 45, origin=(0, 0))                      # repere tourne : traits horizontaux
    for k in range(-60, 60):
        y0 = k * PITCH
        band = rz.intersection(box(-300, y0, 300, y0 + W))
        for g in getattr(band, "geoms", [band]):
            if g.geom_type != "Polygon":
                continue
            for i in g.interiors:                                       # obstacle au milieu d'un trait : on coupe le trait
                g = g.difference(box(i.bounds[0], y0 - 1, i.bounds[2], y0 + W + 1))
            for q in getattr(g, "geoms", [g]):
                q = q.buffer(-.3).buffer(.3)                            # pas de languettes < 0,6 mm
                for r in getattr(q, "geoms", [q]):
                    if r.geom_type == "Polygon" and r.area > 3 and not r.interiors:
                        STRIPES.append(affinity.rotate(r, -45, origin=(0, 0)).simplify(.01))
    pts = lambda g: " ".join(f"(xy {x:.4f} {y:.4f})" for x, y in list(g.exterior.coords)[:-1])
    for g in STRIPES:
        for ly in ("F.Mask",) if V == "C" else ("F.Cu", "F.Mask"):
            o.append(f'\t(gr_poly (pts {pts(g)}) (stroke (width 0) (type solid)) (fill yes) (layer "{ly}") (uuid "{uid()}"))')
        if V == "C":    # trait = zone GND : soude au plan de masse qui l'entoure
            o.append(f'\t(zone (net "GND") (layer "F.Cu") (uuid "{uid()}") (hatch none 0.5) (priority 1) (connect_pads yes (clearance 0.5))'
                     f' (min_thickness 0.2) (fill yes (thermal_gap 0.5) (thermal_bridge_width 0.5) (island_removal_mode 1)) (polygon (pts {pts(g)})))')
    for g in TABCU:     # languette : bord + stries, cuivre nu sans net
        for ly in ("F.Cu", "F.Mask"):
            o.append(f'\t(gr_poly (pts {pts(g)}) (stroke (width 0) (type solid)) (fill yes) (layer "{ly}") (uuid "{uid()}"))')
    print(len(STRIPES), "traits deco", len(TABCU), "sur la languette")
for (x0, y0, x1, y1, wd, ly, nt) in tracks:
    o.append(f'\t(segment (start {x0:.4f} {y0:.4f}) (end {x1:.4f} {y1:.4f}) (width {wd}) (layer "{ly}") (net "{nt}") (uuid "{uid()}"))')
if V:       # GND par les zones des 2 faces : vias de couture (pas de 6 mm) partout ou ils tiennent, pour relier les ilots
    other = unary_union([LineString([(a, b), (c, d)]).buffer(wd / 2) for a, b, c, d, wd, _, nt in tracks]
                        + [route_tab.padgeom(p) for p in PADS if p["net"] != "GND" or p["tht"]] + [Point(x, y).buffer(.4) for x, y, _ in vias]
                        + [plate, static["module"], *[Point(m).buffer(1.6) for m in MOUNT], *STRIPES, *ARTP])
    zone = body.buffer(-1.2)
    for gx in range(2, 140, 6):
        for gy in range(2, 118, 6):
            q = Point(gx, gy)
            if zone.contains(q) and other.distance(q) > .4 + .5 + .3:      # via + garde de zone (0,5) + place pour le cuivre
                vias.append((gx, gy, "GND"))
for (x, y, nt) in vias:
    o.append(f'\t(via (at {x:.4f} {y:.4f}) (size 0.8) (drill 0.4) (layers "F.Cu" "B.Cu") (net "{nt}") (uuid "{uid()}"))')
i = s.rindex("\n\t(embedded_fonts")
os.makedirs(g := f"{D}/gerbers", exist_ok=True)
open(PCB, "w", encoding="utf-8").write(s[:i] + "\n" + "\n".join(o) + s[i:])
print("routage : %d pistes, %d vias, echecs : %s" % (len(tracks), len(vias), failed or "aucun"))


def apercu_svg():
    """Apercu fidele (memes donnees que le .kicad_pcb) : face avant nue, face avant montee, dos (vu de derriere, miroir)."""
    sb = body.union(TABG)                                                   # C : carte + languette
    shell = M.path(sb) if sb.geom_type == "Polygon" else "".join(M.path(p) for p in sb.geoms)
    shell += "".join(f"M{x - 1.6},{y}a1.6,1.6 0 1,0 3.2,0a1.6,1.6 0 1,0 -3.2,0Z" for x, y in MOUNT)
    ctr = lambda ref: (sum(p["x"] for p in PADS if p["ref"] == ref) / sum(p["ref"] == ref for p in PADS),
                       sum(p["y"] for p in PADS if p["ref"] == ref) / sum(p["ref"] == ref for p in PADS))
    trk = lambda ly, col, op: "".join(f'<line x1="{a:.2f}" y1="{b:.2f}" x2="{c:.2f}" y2="{d:.2f}" stroke="{col}" stroke-opacity="{op}" '
                                      f'stroke-width="{wd}" stroke-linecap="round"/>' for a, b, c, d, wd, l, _ in tracks if l == ly)

    def pad(p, side):
        if p["tht"] and p["shape"] == "rect":                               # D-trous : pastille rectangulaire percee (trou de 1 mm)
            return (f'<rect x="{p["x"] - p["w"] / 2:.2f}" y="{p["y"] - p["h"] / 2:.2f}" width="{p["w"]}" height="{p["h"]}" rx=".2" fill="url(#gold)"/>'
                    f'<circle cx="{p.get("hx", p["x"]):.2f}" cy="{p["y"]:.2f}" r=".5" fill="#d9dde3"/>')
        if p["tht"]:
            return f'<circle cx="{p["x"]:.2f}" cy="{p["y"]:.2f}" r="{p["w"] / 2}" fill="url(#gold)"/><circle cx="{p["x"]:.2f}" cy="{p["y"]:.2f}" r="{.6 if p["w"] > 3 else .5}" fill="#d9dde3"/>'
        if (side == "F") != bool(p.get("front")):
            return ""
        return f'<rect x="{p["x"] - p["w"] / 2:.2f}" y="{p["y"] - p["h"] / 2:.2f}" width="{p["w"]}" height="{p["h"]}" rx=".2" fill="url(#gold)"/>'

    def board(side, monte=False):
        g = [f'<g filter="url(#sh)"><g filter="url(#rim)"><path fill="url(#pcb)" fill-rule="evenodd" d="{shell}"/></g></g>']
        ly, other = ("F.Cu", "B.Cu") if side == "F" else ("B.Cu", "F.Cu")
        g.append(trk(other, "#06200f", .12))                                   # pistes de l'autre face, vues par transparence
        g.append(trk(ly, "#06200f", .45) + trk(ly, "#38b56d", .55))
        if side == "B":                                                        # QR au dos (colonnes deja en miroir)
            u = QS / len(QM)
            g.append(f'<rect x="{QX - 1.3}" y="{QY - 1.3}" width="{QS + 2.6}" height="{QS + 2.6}" fill="url(#gold)"/><g fill="#0b3b20">'
                     + "".join(f'<rect x="{QX + (len(QM) - 1 - c) * u:.3f}" y="{QY + r * u:.3f}" width="{u + .01:.3f}" height="{u + .01:.3f}"/>'
                               for r in range(len(QM)) for c in range(len(QM)) if QM[r][c]) + "</g>")
        if side == "F":
            g += [f'<path d="{M.path(q)}" fill="url(#gold)"/>' for q in STRIPES + TABCU + ARTP]
        g += [pad(p, side) for p in PADS]
        g += [f'<circle cx="{x:.2f}" cy="{y:.2f}" r=".4" fill="url(#gold)"/><circle cx="{x:.2f}" cy="{y:.2f}" r=".2" fill="#d9dde3"/>' for x, y, _ in vias]
        g += [f'<text x="{x}" y="{y + sz * .36:.2f}" {M.S} font-size="{sz * 1.25:.2f}" text-anchor="middle"'
              + (f' transform="translate({2 * x:.2f} 0) scale(-1 1)"' if sd == "B" else "") + f'>{t}</text>' for t, x, y, sz, sd in SILK if sd == side]
        if side == "B":
            g.append(f'<rect x="{fx - 9}" y="{fy - 11.25}" width="18" height="22.5" fill="none" stroke="#f4f4ee" stroke-width=".2" stroke-dasharray=".8 .5"/>')
        if monte and side == "F":
            g.append(f'<g filter="url(#sh2)"><rect x="{SX}" y="{SY}" width="{M.SW}" height="{M.SH}" rx="1.2" fill="#16181d"/></g>')
            gx, gy, gh = SX + 2.4, SY + 2.4, M.SH - 4.8
            g.append(f'<rect x="{gx}" y="{gy}" width="54.5" height="{gh}" rx=".4" fill="#050607"/>'
                     f'<g clip-path="url(#scr)" transform="translate({gx + 6.7:.2f} {gy + 1.7:.2f}) scale({40.8 / 320:.5f})">{M.SCENE}</g>')
            for ref, cs in (("SW1", 6), ("SW2", 6), ("SW3", 12)):
                cx, cy = ctr(ref)
                cap = f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{cs * .31:.2f}" fill="url(#{"capR" if cs == 12 else "capK"})"/>'
                g.append(f'<g filter="url(#sh2)"><rect x="{cx - cs / 2:.2f}" y="{cy - cs / 2:.2f}" width="{cs}" height="{cs}" rx=".5" fill="url(#metal)"/>{cap}</g>')
            for xt, yr, yd in LEDS:                                                 # LED 1206 (option) + resistance
                g.append(f'<rect x="{xt - 1.6:.2f}" y="{yr - .8:.2f}" width="3.2" height="1.6" fill="#1b1b1b"/>'
                         f'<rect x="{xt - 1.6:.2f}" y="{yd - .8:.2f}" width="3.2" height="1.6" fill="#f2f2ea"/><rect x="{xt - 1.0:.2f}" y="{yd - .6:.2f}" width="2" height="1.2" fill="#ff3b2a" opacity=".85"/>')
        if monte and side == "B":
            g.append(f'<g filter="url(#sh2)"><rect x="{fx - 9}" y="{fy - 11.25}" width="18" height="22.5" rx=".8" fill="#1d1f24"/>'
                     f'<rect x="{fx - 4.5}" y="{fy + 7.5}" width="9" height="4.2" rx="1.2" fill="url(#metal)"/>'
                     f'<rect x="{fx - 3}" y="{fy - 6}" width="6" height="6" fill="#33363d"/></g>')
        return "".join(g)

    views = [("Face avant (carte nue)", board("F"), False), ("Face avant (montée)", board("F", True), False),
             ("Dos (vu de derrière)", board("B", True), True)]
    bx1, by1 = body.bounds[2:]
    ty = 10 - min(0, sb.bounds[1])                                          # C : la languette depasse au-dessus de y = 0
    bw, bh = bx1 - body.bounds[0], by1 - sb.bounds[1]
    pw, ph = bx1 + 9, by1 + 8 + ty                                          # une vue par panneau
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="100%" viewBox="0 0 {3 * pw:.1f} {ph:.1f}">', M.DEFS,
           f'<rect width="{3 * pw:.1f}" height="{ph:.1f}" fill="#d9dde3"/>']
    for k, (titre, g, mir) in enumerate(views):
        tf = f"translate({k * pw + 3 + (bx1 + 1.5 if mir else 0):.1f} {ty:.1f})" + (" scale(-1 1)" if mir else "")
        out.append(f'<text x="{k * pw + pw / 2:.1f}" y="6" font-family="Arial,Helvetica,sans-serif" font-size="3.4" font-weight="700" fill="#2c3038" text-anchor="middle">{titre}</text>')
        out.append(f'<g transform="{tf}">{g}</g>')
    out.append(f'<text x="{1.5 * pw:.1f}" y="{ph - 3:.1f}" font-family="Arial,Helvetica,sans-serif" font-size="2.2" fill="#5a606b" text-anchor="middle">'
               f'{NAME} - {bw:.0f} x {bh:.0f} mm - {len(H)} ajours - pistes claires = face visible, foncées = autre face</text></svg>')
    open(f"{D}/{NAME}.svg", "w", encoding="utf-8").write("\n".join(out))


if V:
    apercu_svg()

run = lambda *a: subprocess.run([KICAD_CLI, *a], check=True)
for _ in range(4):   # vias de couture tombes sur un ilot de zone supprime : on les retire et on refait la DRC
    if os.path.exists(f"{D}/drc.rpt"):
        os.remove(f"{D}/drc.rpt")                                          # kicad-cli rend 0 meme s'il n'a pas pu lire le PCB
    run("pcb", "drc", "--refill-zones", "--save-board", "--output", f"{D}/drc.rpt", PCB)
    rpt = open(f"{D}/drc.rpt", encoding="utf-8").read()
    bad = re.findall(r"\[via_dangling\].*?\n.*?\n\s*@\(([\d.]+) mm, ([\d.]+) mm\): Via \[GND\]", rpt)
    iso = [Point(float(x), float(y)) for x, y in
           re.findall(r"\[isolated_copper\].*?\n.*?\n\s*@\(([-\d.]+) mm, ([-\d.]+) mm\): Zone \[GND\] on F.Cu, priority [1-9]\d*", rpt)]
    add = []
    if "[unconnected_items]" in rpt and re.search(r"\[unconnected_items\].*?\n.*?\n\s*@\([^)]*\): Zone \[GND\]", rpt):
        t = open(PCB, encoding="utf-8").read()        # morceau de plan GND sans via (relie seulement a une pastille) : via au milieu
        fp = [(ly, Polygon([tuple(map(float, m)) for m in re.findall(r"\(xy ([-\d.]+) ([-\d.]+)\)", pp)]))
              for ly, pp in re.findall(r'\(filled_polygon\s*\(layer "(.\.Cu)"\)\s*\(pts(.*?)\)\s*\)', t, re.S)]
        vg = [Point(float(a), float(b)) for a, b in re.findall(r"\n\t\(via\n\t\t\(at ([-\d.]+) ([-\d.]+)\)(?:(?!\n\t\)).)*?\(net \"GND\"\)", t, re.S)]
        for ly, p in fp:
            if p.area > 2 and not any(p.buffer(.05).contains(v) for v in vg):
                c = p.buffer(-.9).intersection(unary_union([q for l2, q in fp if l2 != ly]).buffer(-.9))   # .9 = via .4 + garde .5
                if not c.is_empty:
                    q = c.representative_point()
                    add.append((round(q.x, 3), round(q.y, 3)))
    if not bad and not iso and not add:
        break
    t = open(PCB, encoding="utf-8").read()
    i = t.rindex("\n\t(embedded_fonts")
    t = t[:i] + "".join(f'\n\t(via (at {x} {y}) (size 0.8) (drill 0.4) (layers "F.Cu" "B.Cu") (net "GND") (uuid "{uid()}"))' for x, y in add) + t[i:]
    if add:
        print("vias ajoutes (plan GND isole) :", add)
    for z in re.findall(r"\n\t\(zone\n.*?\n\t\)(?=\n)", t, re.S):        # C : strie sans contact avec le plan de masse -> cuivre sans net
        if re.search(r"\(priority [1-9]\d*\)", z):
            pp = re.search(r"\(polygon\s*\(pts(.*?)\)\s*\)", z, re.S).group(1)
            if any(Polygon([tuple(map(float, m)) for m in re.findall(r"\(xy ([-\d.]+) ([-\d.]+)\)", pp)]).buffer(.01).contains(q) for q in iso):
                t = t.replace(z, f'\n\t(gr_poly (pts {" ".join(pp.split())})) (stroke (width 0) (type solid)) (fill yes) (layer "F.Cu") (uuid "{uid()}"))', 1)
    if iso:
        print("stries isolees -> cuivre sans net :", len(iso))
    for x, y in bad:
        t = re.sub(rf"\n\t\(via\n\t\t\(at {float(x):g} {float(y):g}\)\n.*?\n\t\)", "", t, count=1, flags=re.S)
    open(PCB, "w", encoding="utf-8").write(t)
    print("vias de couture retires :", bad)
for f in glob.glob(g + "/*"):
    os.remove(f)
run("pcb", "export", "gerbers", "--output", g, "--layers", "F.Cu,B.Cu,F.Mask,B.Mask,F.SilkS,B.SilkS,F.Paste,B.Paste,Edge.Cuts", "--board-plot-params", PCB)
run("pcb", "export", "drill", "--output", g + "/", "--format", "excellon", "--drill-origin", "absolute", "--excellon-separate-th", PCB)
with zipfile.ZipFile(f"{D}/{NAME}-gerbers.zip", "w", zipfile.ZIP_DEFLATED) as z:
    for f in glob.glob(g + "/*"):
        z.write(f, os.path.basename(f))
