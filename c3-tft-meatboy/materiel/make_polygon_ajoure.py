"""Genere polygon-ajoure-{A,B,C}.svg : PCB au contour polygon.svg (110 x 75.3 mm), meme implantation que
pcb-polygon/ (ecran, boutons, trous M3), + QR code en cuivre, 3 facons d'ajourer.
Usage : python make_polygon_ajoure.py"""
import math, qrcode
from shapely.geometry import Polygon, Point, box, LineString
from shapely.ops import unary_union
from shapely import affinity

URL = "https://github.com/imajon/ClaudeRepo"   # TODO: remplacer par l'URL de la plateforme de modifications
OUT = Polygon([(1.5, 31.52), (1.5, 46.75), (14.98, 54.18), (14.98, 69.22), (28.80, 76.84), (42.61, 69.23), (56.42, 76.84),
               (70.22, 69.23), (84.02, 76.84), (97.84, 69.22), (97.84, 54.29), (111.51, 46.75), (111.51, 31.52), (98.11, 24.13),
               (98.11, 9.12), (84.29, 1.5), (70.49, 9.11), (56.69, 1.5), (42.87, 9.11), (29.06, 1.5), (15.24, 9.12), (15.24, 23.95)])
SX, SY, SW, SH = 25.5, 10.76, 62.0, 37.1
SW1, SW2, SW3 = (23.75, 55.7), (35.75, 60.7), (72.75, 58.7)       # DROITE/GAUCHE 6x6, SAUT 12x12
MOUNT = [(20, 15), (94, 15), (20, 63), (94, 63)]
QX, QY, QS = 45.5, 52.5, 14.0                                    # QR cuivre (bas, centre)
HDR_X = 85.55
# 4 pastilles de fil (2 par cote) sur les GPIO restes libres du SuperMini (utilises : 1-7 et 10 ; GPIO9 = bouton BOOT, laisse libre)
PADS = [(5.0, 35.2, "L"), (5.0, 43.2, "L"), (108.0, 35.2, "R"), (108.0, 43.2, "R")]
PADS_L = [(5.0, 35.2, "L", "IO21"), (5.0, 43.2, "L", "IO20"), (108.0, 35.2, "R", "IO0"), (108.0, 43.2, "R", "IO8")]

KEEP = unary_union([box(SX - 1, SY - 1, SX + SW + 1, SY + SH + 1), Point(SW1).buffer(5), Point(SW2).buffer(5),
                    box(SW3[0] - 9, SW3[1] - 9, SW3[0] + 9, SW3[1] + 9), box(QX - 2.5, QY - 3, QX + QS + 2.5, QY + QS + 2),
                    box(87, 18, 96, 60), *[Point(m).buffer(4.5) for m in MOUNT]])
FREE = OUT.buffer(-2.4).difference(KEEP)


def holes(v):
    if v == "A":      # nid d'abeilles
        r, wall = 3.3, 1.5
        a = r * math.sqrt(3) / 2
        px, py = 2 * a + wall, 1.5 * r + wall * .866
        out = []
        for j in range(-2, 16):
            for i in range(-2, 20):
                cx, cy = 1.5 + i * px + (px / 2 if j % 2 else 0), 1.5 + j * py
                h = Polygon([(cx + r * math.cos(math.radians(90 + 60 * k)), cy - r * math.sin(math.radians(90 + 60 * k))) for k in range(6)])
                if h.within(FREE):
                    out.append(h)
        return out
    if v == "B":      # perforations rondes
        out = []
        p = 4.6
        for j in range(-2, 20):
            for i in range(-2, 28):
                c = Point(1.5 + i * p + (p / 2 if j % 2 else 0), 1.5 + j * p * .866).buffer(1.7)
                if c.within(FREE):
                    out.append(c)
        return out
    # C : fenetres suivant le contour (zones libres erodees puis arrondies)
    g = FREE.buffer(-1.3).buffer(1.0)
    return [p for p in (g.geoms if hasattr(g, "geoms") else [g]) if p.area > 8]


FONT = "C:/Windows/Fonts/ariblk.ttf"  # Arial Black : traits epais = lettres decoupees solides
OVERLAP = .90                            # 1 = lettres juste cote a cote, <1 = elles se chevauchent
SCREEN_OVERLAP = 5                       # mm de lettres cachees derriere le haut de l'ecran
TALL = 1.4                               # etirement vertical des lettres (lettres plus grosses)
ZIG = [(15.24, 9.12), (29.06, 1.5), (42.87, 9.11), (56.69, 1.5), (70.49, 9.11), (84.29, 1.5), (98.11, 9.12)]


LETTERS_SVG = "../../_images/textePCB.svg"   # lettres fournies (Inkscape) : noir = vernis, blanc = cuivre
BODY_ID = "path960"                     # chemin noir sans trait = forme du PCB (lettres + corps)


def svg_letters(svgfile):
    """Lettres du SVG, dans l'ordre d'empilement : liste de (cuivre, vernis ou None).
    Les chemins au trait noir (1re couche) sont ignores. Blanc = cuivre, noir = vernis :
      remplissage noir + trait blanc -> lettre vernie cernee d'un bord cuivre ;
      remplissage blanc              -> lettre entierement en cuivre ;
      trait blanc seul               -> bord cuivre (la lettre dessous grossit d'autant)."""
    from svgelements import SVG, Path
    import numpy as np
    white = lambda c: c is not None and c.value is not None and (c.red or 0) >= 240 and (c.green or 0) >= 240
    out = []
    for e in SVG.parse(svgfile).elements():
        if not isinstance(e, Path) or e.id == BODY_ID or not (white(e.stroke) or white(e.fill)):
            continue
        e = Path(e * e.transform) if e.transform else e
        polys = []
        for sp in e.as_subpaths():
            sp = Path(sp)
            pts = sp.npoint(np.linspace(0, 1, 12 * max(1, len(sp)) + 1))
            polys.append(Polygon(pts).buffer(0))
        g = None
        for p in sorted(polys, key=lambda p: -p.area):      # pair/impair : a, o... gardent leur contreforme
            g = p if g is None else g.symmetric_difference(p)
        if white(e.fill):
            out.append((g, None))
        elif white(e.stroke) and e.fill.value is None:
            out.append((g.buffer((e.stroke_width or 0) / 2, join_style=1), None))
        else:
            out.append((g.buffer((e.stroke_width or 0) / 2, join_style=1), g))
    return out


def svg_body(svgfile):
    from svgelements import SVG, Path
    import numpy as np
    for e in SVG.parse(svgfile).elements():
        if isinstance(e, Path) and e.id == BODY_ID:
            g = None
            polys = [Polygon(Path(sp).npoint(np.linspace(0, 1, 12 * max(1, len(Path(sp))) + 1))).buffer(0) for sp in e.as_subpaths()]
            for p in sorted(polys, key=lambda p: -p.area):
                g = p if g is None else g.symmetric_difference(p)
            return g
    raise SystemExit(f"{BODY_ID} introuvable dans {svgfile}")


def tab_geoms():
    """Le PCB prend exactement la forme du SVG (contour des lettres + corps) ; le blanc du SVG devient du cuivre.
    Mise a l'echelle : la pointe gauche/droite du corps retombe sur (1.5, 31.52)-(111.51, 31.52), comme OUT.
    Retourne (corps, D, cap, [(cuivre, vernis) par lettre])."""
    k = (111.513224 - 1.5) / (170.61388 - 5.7752544)
    u = 25.4 / 96                                       # svgelements rend en px (96 ppi) : retour aux mm du fichier
    tf = lambda q: affinity.affine_transform(q, [k * u, 0, 0, k * u, 1.5 - 5.7752544 * k, 31.519 - 128.1633 * k])
    body = tf(svg_body(LETTERS_SVG))
    ls = [(tf(c), tf(v) if v is not None else None) for c, v in svg_letters(LETTERS_SVG)]
    return body, 0, body.bounds[3] - body.bounds[1], ls


def FontProp(f, size=10):
    from matplotlib.font_manager import FontProperties
    return FontProperties(fname=f, size=size)


def path(p):
    return "M" + " L".join(f"{x:.2f},{y:.2f}" for x, y in p.exterior.coords) + "Z" + "".join(
        "M" + " L".join(f"{x:.2f},{y:.2f}" for x, y in i.coords) + "Z" for i in p.interiors)


S = 'fill="#f4f4ee" font-family="Arial,Helvetica,sans-serif" font-weight="700"'

DEFS = '''<defs>
<linearGradient id="pcb" gradientUnits="userSpaceOnUse" x1="0" y1="0" x2="112" y2="78"><stop offset="0" stop-color="#1f8a4c"/><stop offset=".55" stop-color="#136b3a"/><stop offset="1" stop-color="#0c5230"/></linearGradient>
<linearGradient id="gold" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#f3dc8e"/><stop offset=".5" stop-color="#c9a24a"/><stop offset="1" stop-color="#8f6d22"/></linearGradient>
<linearGradient id="metal" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#e6e9ee"/><stop offset=".5" stop-color="#a9b0b9"/><stop offset="1" stop-color="#6d747d"/></linearGradient>
<radialGradient id="capK" cx=".35" cy=".3" r=".8"><stop offset="0" stop-color="#5a5f68"/><stop offset="1" stop-color="#0e1013"/></radialGradient>
<radialGradient id="capR" cx=".35" cy=".3" r=".85"><stop offset="0" stop-color="#ff8f7d"/><stop offset=".5" stop-color="#d8261a"/><stop offset="1" stop-color="#7a0f0a"/></radialGradient>
<linearGradient id="sky" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#2a1f5c"/><stop offset="1" stop-color="#d9628a"/></linearGradient>
<linearGradient id="glare" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#fff" stop-opacity=".28"/><stop offset=".45" stop-color="#fff" stop-opacity=".04"/><stop offset=".46" stop-color="#fff" stop-opacity="0"/></linearGradient>
<filter id="sh" x="-10%" y="-10%" width="125%" height="130%"><feDropShadow dx=".8" dy="1.4" stdDeviation="1.1" flood-opacity=".45"/></filter>
<filter id="sh2" x="-30%" y="-30%" width="170%" height="170%"><feDropShadow dx=".25" dy=".45" stdDeviation=".3" flood-opacity=".6"/></filter>
<filter id="rim" x="-2%" y="-2%" width="104%" height="104%"><feMorphology in="SourceAlpha" operator="erode" radius=".4" result="e"/><feComposite in="SourceAlpha" in2="e" operator="out" result="r"/><feFlood flood-color="#7fd9a0" flood-opacity=".5"/><feComposite in2="r" operator="in" result="rr"/><feMerge><feMergeNode in="SourceGraphic"/><feMergeNode in="rr"/></feMerge></filter>
<clipPath id="scr"><rect width="320" height="240"/></clipPath></defs>'''

SCENE = ('<rect width="320" height="240" fill="url(#sky)"/>'
         '<polygon points="0,170 50,95 95,150 140,80 200,165 250,110 320,170" fill="#4a2c6e" opacity=".85"/><polygon points="140,80 128,100 140,96 152,102" fill="#f2e9f7"/><polygon points="250,110 240,126 250,122 260,128" fill="#f2e9f7"/>'
         '<rect y="196" width="320" height="44" fill="#6b3b1f"/><rect y="196" width="320" height="9" fill="#3fbf4f"/>'
         + "".join(f'<rect x="{x}" y="{y}" width="{wd}" height="10" fill="#8b5a2b"/><rect x="{x}" y="{y}" width="{wd}" height="4" fill="#3fbf4f"/>' for x, y, wd in [(40, 160, 70), (140, 130, 60), (230, 100, 70)])
         + '<polygon points="272,96 296,78 296,96" fill="#ffd23f"/><rect x="293" y="62" width="3" height="34" fill="#ddd"/><polygon points="296,62 312,69 296,76" fill="#ffd23f"/>'
         '<rect x="58" y="136" width="24" height="24" fill="#d1121b"/><rect x="62" y="143" width="6" height="6" fill="#fff"/><rect x="72" y="143" width="6" height="6" fill="#fff"/><rect x="64" y="145" width="3" height="3"/><rect x="74" y="145" width="3" height="3"/>'
         '<circle cx="172" cy="176" r="20" fill="#c9ced6"/><circle cx="172" cy="176" r="6" fill="#555"/>'
         '<polygon points="170,100 174,111 186,111 176,118 180,129 170,122 160,129 164,118 154,111 166,111" fill="#ffd23f"/>'
         '<rect width="320" height="22" opacity=".55"/><text x="8" y="16" font-family="monospace" font-weight="700" font-size="14" fill="#fff">NIV 7   0:23</text><text x="312" y="16" font-family="monospace" font-weight="700" font-size="14" fill="#ffd23f" text-anchor="end">★★☆</text>')


def build(v):
    T = v == "T"
    H = [] if T else holes(v)
    D, VH = 0, 90
    if T:
        body, D, cap, ls = tab_geoms()
        VH = 90 + D
    o = []
    w = o.append
    w(f'<svg xmlns="http://www.w3.org/2000/svg" width="100%" viewBox="-6 -6 124 {VH}">')
    w(DEFS)
    w(f'<rect x="-6" y="-6" width="124" height="{VH}" fill="{'#9aa1ac' if T else '#d9dde3'}"/>')
    if T:
        shell = "".join(path(p) for p in (body.geoms if hasattr(body, "geoms") else [body]))
    else:
        shell = path(OUT) + "".join(path(h) for h in H)
    w(f'<g filter="url(#sh)"><g filter="url(#rim)"><path fill="url(#pcb)" fill-rule="evenodd" d="{shell}'
      + "".join(f"M{x-1.6},{y+D*T} a1.6,1.6 0 1,0 3.2,0 a1.6,1.6 0 1,0 -3.2,0Z" for x, y in MOUNT) + f'"/><g transform="translate(0 {D*T})">')
    for i in range(8):     # bus de l'ecran vers les boutons
        py = SY + SH / 2 - 8.89 + i * 2.54
        bx = 93.4 - 0.7 * i
        yh = 57.4 - 0.6 * i
        d = f"M{HDR_X},{py:.2f} H{bx:.2f} V{yh:.2f} H76"
        w(f'<path d="{d}" fill="none" stroke="#06200f" stroke-opacity=".5" stroke-width=".75" stroke-linejoin="round"/>'
          f'<path d="{d}" fill="none" stroke="#38b56d" stroke-opacity=".75" stroke-width=".42" stroke-linejoin="round"/>')
    for (px, py, side) in PADS:     # pistes des pastilles vers l'ESP32 (au dos, sous l'ecran) : segment avant, via, segment arriere
        dx = 1 if side == "L" else -1
        xe = 25.0 if side == "L" else 95.0
        w(f'<path d="M{px},{py} H{xe}" fill="none" stroke="#06200f" stroke-opacity=".5" stroke-width=".75" stroke-linecap="round"/>'
          f'<path d="M{px},{py} H{xe}" fill="none" stroke="#38b56d" stroke-opacity=".75" stroke-width=".42" stroke-linecap="round"/>')
    w('</g></g></g>')
    if T:   # chaque lettre : bord cuivre (blanc du SVG) puis vernis par-dessus -> la lettre suivante recouvre la precedente
        w(f'<clipPath id="shell"><path clip-rule="evenodd" d="{shell}"/></clipPath>')
        pth = lambda z: "".join(path(p) for p in (z.geoms if hasattr(z, "geoms") else [z]))
        for cu, gl in ls:
            w(f'<path d="{pth(cu)}" fill="url(#gold)" fill-rule="evenodd" clip-path="url(#shell)"/>')
            if gl is not None:
                w(f'<path d="{pth(gl)}" fill="url(#pcb)" fill-rule="evenodd" clip-path="url(#shell)"/>')
    w(f'<g transform="translate(0 {D*T})">')
    for x, y in MOUNT:
        w(f'<circle cx="{x}" cy="{y}" r="2.6" fill="none" stroke="url(#gold)" stroke-width=".7"/>')
    for px, py, side, name in PADS_L:
        w(f'<circle cx="{px}" cy="{py}" r="1.9" fill="url(#gold)" filter="url(#sh2)"/><circle cx="{px}" cy="{py}" r=".6" fill="#0b2a16"/>')
        tx, an = (px + 3.1, "start") if side == "L" else (px - 3.1, "end")
        w(f'<text x="{tx}" y="{py + .6}" {S} font-size="1.7" text-anchor="{an}">{name}</text>')
    for px, py, side in PADS:       # cote droit : via avant le bus de l'ecran, la piste repasse au dos sous le bus
        if side == "R":
            w(f'<circle cx="95" cy="{py}" r=".75" fill="url(#gold)"/><circle cx="95" cy="{py}" r=".3" fill="#0b2a16"/>')
    # QR cuivre : plaque cuivre (clair) + modules en vernis (sombre) -> polarite normale
    q = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M, border=0)
    q.add_data(URL)
    q.make(fit=True)
    m = q.get_matrix()
    n = len(m)
    u = QS / n
    w(f'<g filter="url(#sh2)"><rect x="{QX-1.3}" y="{QY-1.3}" width="{QS+2.6}" height="{QS+2.6}" rx=".5" fill="url(#gold)"/></g>')
    w('<g fill="#0b3b20">' + "".join(f'<rect x="{QX+c*u:.3f}" y="{QY+r*u:.3f}" width="{u+.01:.3f}" height="{u+.01:.3f}"/>'
                                      for r in range(n) for c in range(n) if m[r][c]) + '</g>')
    w(f'<text x="{QX+QS/2}" y="{QY-2.0}" {S} font-size="1.5" text-anchor="middle">SCAN · MODS</text>')
    # ecran
    w(f'<g filter="url(#sh2)"><rect x="{SX}" y="{SY}" width="{SW}" height="{SH}" rx="1.2" fill="#16181d"/>'
      f'<rect x="{SX+.4}" y="{SY+.4}" width="{SW-.8}" height="{SH-.8}" rx=".9" fill="none" stroke="#2c3038" stroke-width=".3"/></g>')
    gx, gy, gw, gh = SX + 2.4, SY + 2.4, 54.5, SH - 4.8
    ax, ay, aw = gx + 6.7, gy + 1.7, 40.8
    w(f'<rect x="{gx}" y="{gy}" width="{gw}" height="{gh}" rx=".4" fill="#050607" stroke="#3a3f48" stroke-width=".25"/>')
    w(f'<g clip-path="url(#scr)" transform="translate({ax:.2f} {ay:.2f}) scale({aw/320:.5f})">{SCENE}</g>')
    w(f'<polygon points="{gx},{gy} {gx+gw*.55:.2f},{gy} {gx+gw*.3:.2f},{gy+gh} {gx},{gy+gh}" fill="url(#glare)" opacity=".9"/>')
    w(f'<rect x="{HDR_X-1.3}" y="{SY+SH/2-10.4:.2f}" width="2.6" height="20.8" rx=".2" fill="#111"/>')
    for i in range(8):
        w(f'<circle cx="{HDR_X}" cy="{SY+SH/2-8.89+i*2.54:.2f}" r=".65" fill="url(#gold)"/>')
    # boutons
    for (cx, cy), lab in ((SW1, "GAUCHE"), (SW2, "DROITE")):
        w(f'<rect x="{cx-3.9}" y="{cy-3.9}" width="7.8" height="7.8" fill="none" stroke="#f4f4ee" stroke-width=".28"/>')
        for dx in (-3.9, 3.9):
            for dy in (-2.25, 2.25):
                w(f'<rect x="{cx+dx-.45}" y="{cy+dy-.35}" width=".9" height=".7" fill="url(#gold)"/>')
        w(f'<g filter="url(#sh2)"><rect x="{cx-3}" y="{cy-3}" width="6" height="6" rx=".4" fill="url(#metal)"/><circle cx="{cx}" cy="{cy}" r="1.9" fill="url(#capK)"/><circle cx="{cx-.55}" cy="{cy-.6}" r=".5" fill="#fff" opacity=".35"/></g>')
        w(f'<text x="{cx}" y="{cy+6.9}" {S} font-size="1.7" text-anchor="middle">{lab}</text>')
    cx, cy = SW3
    w(f'<rect x="{cx-7.2}" y="{cy-7.2}" width="14.4" height="14.4" fill="none" stroke="#f4f4ee" stroke-width=".3"/>')
    for dx in (-6.9, 6.9):
        for dy in (-2.5, 2.5):
            w(f'<rect x="{cx+dx-.8}" y="{cy+dy-.45}" width="1.6" height=".9" fill="url(#gold)"/>')
    w(f'<g filter="url(#sh2)"><rect x="{cx-6}" y="{cy-6}" width="12" height="12" rx=".7" fill="url(#metal)"/><rect x="{cx-5.2}" y="{cy-5.2}" width="10.4" height="10.4" rx=".5" fill="#2a2d33"/>'
      f'<circle cx="{cx}" cy="{cy}" r="3.7" fill="url(#capR)" stroke="#5a0b07" stroke-width=".2"/><ellipse cx="{cx-1.2}" cy="{cy-1.5}" rx="1.3" ry=".9" fill="#fff" opacity=".4" transform="rotate(-35 {cx-1.2} {cy-1.5})"/></g>')
    w(f'<text x="{cx}" y="{cy+10.4}" {S} font-size="2.2" text-anchor="middle">SAUT</text>')
    w('</g></svg>')
    open(f"polygon-ajoure-{v}.svg" if not T else "polygon-tab-lettres.svg", "w", encoding="utf-8").write("\n".join(o))
    print(v, len(H), "trous", D)


if __name__ == "__main__":
    for v in "T":
        build(v)
