#!/usr/bin/env python3
"""Fiche d'assemblage de la console livree (carte polygon ajoure D) : dans quel sens poser l'ESP32-C3 (verso),
l'ecran et les boutons (recto). Carte dessinee depuis le .kicad_pcb (make_cadre_apercu.side), pistes retirees pour la lisibilite.
Sortie : fiche-assemblage-D.svg (A4) + meme fiche dans brique/index.html entre les marqueurs FICHE-ASSEMBLAGE.
Usage : py -3.13 make_fiche_assemblage.py   (shapely requis)"""
import re, runpy, sys
from pathlib import Path

ICI = Path(__file__).resolve().parent
sys.argv = ['', 'pcb-polygon-ajoure-D', 'c3-tft-meatboy-polygon-ajoure-D', 'recto']   # reecrit le -recto.svg a l'identique
G = runpy.run_path(str(ICI / 'make_cadre_apercu.py'), run_name='__main__')
X0, Y0, X1, Y1 = G['shell'].bounds
DEFS = G['M'].DEFS

# implantation (mm, repere KiCad = vu de face) lue dans pcb-polygon-ajoure-D/*.kicad_pcb
SX, SY, SW, SH = 25.5, 10.76, 62.0, 37.1                 # module ecran
J3X, J3Y = 85.55, 20.42                                   # barrette ecran, broche 1 en haut, pas 2,54
ECRAN = ['GND', 'VCC', 'SCL', 'SDA', 'RES', 'DC', 'CS', 'BL']
BOUTONS = [('Gauche', 28.0, 57.95, 6), ('Droite', 40.0, 62.95, 6), ('Saut', 80.0, 61.2, 12)]
FX, FY = 56.42, 61.75                                     # ESP32-C3 SuperMini (18 x 22,5), USB-C en bas (bord plat y = 73)
ESP_G = ['0', '1', '2', '3', '4', '3.3', 'G', '5V']       # colonne vue a GAUCHE depuis le verso, haut -> bas
ESP_D = ['21', '20', '10', '9', '8', '7', '6', '5']
VERT, ROUGE, BLEU, NUIT = '#15a34a', '#dc2626', '#1d4ed8', '#12233a'

W, H = 1000, 1414
o = []
a = o.append


def txt(x, y, s, size=13, w=400, fill='#222', anchor='start'):
    a(f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" font-weight="{w}" fill="{fill}" text-anchor="{anchor}">{s}</text>')


def titre(y, s, sous=''):
    a(f'<rect x="30" y="{y - 22}" width="940" height="32" rx="6" fill="{NUIT}"/>')
    txt(44, y, s, 17, 800, '#fff')
    if sous:
        txt(956, y, sous, 12, 400, '#a8b8cc', 'end')


def puces(x, y, lignes, size=13.5, dy=21):
    for i, s in enumerate(lignes):
        a(f'<circle cx="{x + 4}" cy="{y + i * dy - 4.5}" r="3" fill="{NUIT}"/>')
        txt(x + 14, y + i * dy, s, size, 400, '#222')


def fleche(x1, y1, x2, y2, col=ROUGE, w=3):
    import math
    t = math.atan2(y2 - y1, x2 - x1)
    p = [(x2, y2), (x2 - 11 * math.cos(t - .45), y2 - 11 * math.sin(t - .45)), (x2 - 11 * math.cos(t + .45), y2 - 11 * math.sin(t + .45))]
    a(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2 - 7 * math.cos(t):.1f}" y2="{y2 - 7 * math.sin(t):.1f}" stroke="{col}" stroke-width="{w}" stroke-linecap="round"/>'
      f'<polygon points="{" ".join(f"{u:.1f},{v:.1f}" for u, v in p)}" fill="{col}"/>')


class Vue:
    """Une face de la carte a l'echelle k, coin haut-gauche en (ox, oy) ; verso = vu de derriere (miroir)."""
    def __init__(self, face, ox, oy, k):
        self.f, self.ox, self.oy, self.k, self.m = face, ox, oy, k, face == 'B'
        g = re.sub(r'<line [^>]*stroke="#06200f"[^>]*/>', '', G['side'](face, self.m))                           # pistes
        g = re.sub(r'<circle [^>]*r="\.4" fill="url\(#gold\)"/><circle [^>]*r="\.2"[^>]*/>', '', g)          # vias
        tf = f'translate({ox} {oy}) scale({k}) ' + (f'translate({X1} {-Y0}) scale(-1 1)' if self.m else f'translate({-X0} {-Y0})')
        a(f'<g transform="{tf}">{g}</g>')

    def p(self, x, y):
        return self.ox + self.k * ((X1 - x) if self.m else (x - X0)), self.oy + self.k * (y - Y0)

    def rect(self, x, y, w, h, **at):
        (u1, v1), (u2, v2) = self.p(x, y), self.p(x + w, y + h)
        attrs = ' '.join(f'{k.replace("_", "-")}="{v}"' for k, v in at.items())
        a(f'<rect x="{min(u1, u2):.1f}" y="{v1:.1f}" width="{abs(u2 - u1):.1f}" height="{v2 - v1:.1f}" {attrs}/>')


def esp(cx, cy, k, puce=True, usb_bas=True, broches=False):
    """ESP32-C3 SuperMini vu de dessus (page), centre (cx, cy), k px/mm. puce=False : retourne (dos noir uni)."""
    w, h, s = 18 * k, 22.5 * k, 1 if usb_bas else -1
    a(f'<rect x="{cx - w / 2:.1f}" y="{cy - h / 2:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{.8 * k:.1f}" fill="#1d1f24" stroke="#000" stroke-width="1"/>')
    for i in range(8):                                                       # demi-trous des bords
        for sx in (-1, 1):
            a(f'<circle cx="{cx + sx * 7.62 * k:.1f}" cy="{cy + (-8.89 + 2.54 * i) * k:.1f}" r="{.55 * k:.1f}" fill="#d4ad4f"/>')
    if puce:
        cc = cy - s * 3 * k                                                  # puce, cote oppose a l'USB
        a(f'<rect x="{cx - 3 * k:.1f}" y="{cc - 3 * k:.1f}" width="{6 * k:.1f}" height="{6 * k:.1f}" fill="#3a3e46" stroke="#555a63" '
          f'stroke-width=".8" transform="rotate(45 {cx:.1f} {cc:.1f})"/>')
        for bx in (-2.6, 2.6):                                               # BOOT / RST
            a(f'<rect x="{cx + bx * k - 1.4 * k:.1f}" y="{cy + s * 3.6 * k - 1.2 * k:.1f}" width="{2.8 * k:.1f}" height="{2.4 * k:.1f}" rx="1" fill="#c9ced6"/>')
        a(f'<rect x="{cx - 1.2 * k + 5.2 * k:.1f}" y="{cy - s * 9 * k - .6 * k:.1f}" width="{2.4 * k:.1f}" height="{1.2 * k:.1f}" fill="#e43"/>')
    else:
        txt(cx, cy + 4, 'dos', max(9, k * 2.6), 700, '#8a8f99', 'middle')
    a(f'<rect x="{cx - 4.5 * k:.1f}" y="{cy + s * 7.6 * k - (0 if s > 0 else 4.2 * k):.1f}" width="{9 * k:.1f}" height="{4.2 * k:.1f}" rx="{1.2 * k:.1f}" fill="#b9bfc8" stroke="#6d747d"/>')
    if broches:
        for i, (g, d) in enumerate(zip(ESP_G, ESP_D)):
            y = cy + (-8.89 + 2.54 * i) * k + 3.5
            txt(cx - 7.62 * k + 1.1 * k, y, g, 9.5, 700, '#fff')
            txt(cx + 7.62 * k - 1.1 * k, y, d, 9.5, 700, '#fff', 'end')


def etiquette(x, y, lignes, col=ROUGE, anchor='middle', size=13):
    """Texte sur fond blanc (lisible sur la carte), y = ligne de base de la 1re ligne."""
    w = max(len(l) for l in lignes) * size * .56 + 12
    x0 = x - w / 2 if anchor == 'middle' else x - w if anchor == 'end' else x
    a(f'<rect x="{x0:.1f}" y="{y - size - 3:.1f}" width="{w:.1f}" height="{len(lignes) * (size + 3) + 6:.1f}" rx="5" fill="#fff" fill-opacity=".93" stroke="{col}" stroke-width="1.5"/>')
    for i, l in enumerate(lignes):
        txt(x0 + w / 2, y + i * (size + 3), l, size, 800, col, 'middle')


def verdict(x, y, ok, s):
    c = VERT if ok else ROUGE
    a(f'<circle cx="{x}" cy="{y}" r="11" fill="{c}"/>')
    txt(x, y + 5, '✓' if ok else '✗', 15, 800, '#fff', 'middle')
    txt(x + 18, y + 5, s, 13, 700, c)


a(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="210mm" height="297mm" font-family="Segoe UI, Arial, sans-serif">')
a(DEFS)
a(f'<rect width="{W}" height="{H}" fill="#fff"/>')
txt(W / 2, 44, 'Console Brique : dans quel sens monter les pièces', 26, 800, NUIT, 'middle')
txt(W / 2, 68, "Carte hexagonale (version D) - l'ESP32 va au VERSO, l'écran et les boutons au RECTO", 14, 400, '#5a6b82', 'middle')

# ---- 1. les deux faces ----
titre(110, '1. Repérer les deux faces', 'le QR code est au verso')
k1 = 4.0
r = Vue('F', 35, 140, k1)
r.rect(SX, SY, SW, SH, fill='none', stroke=BLEU, stroke_width=2.5, stroke_dasharray='8 5')
x, y = r.p(SX + SW / 2, SY + SH / 2)
etiquette(x, y + 6, ['ÉCRAN'], BLEU, size=16)
for nom, bx, by, s in BOUTONS:
    r.rect(bx - s / 2, by - s / 2, s, s, fill='none', stroke=BLEU, stroke_width=2, stroke_dasharray='5 3')
txt(*r.p(56, Y1 + 6), 'RECTO : écran + 3 boutons', 16, 800, NUIT, 'middle')
v = Vue('B', 530, 140, k1)
v.rect(FX - 9, FY - 11.25, 18, 22.5, fill='none', stroke=ROUGE, stroke_width=2.5, stroke_dasharray='8 5')
x, y = v.p(FX, FY)
etiquette(x, y + 5, ['ESP32'], ROUGE)
x, y = v.p(J3X, J3Y + 8.89)
a(f'<rect x="{x - 9:.1f}" y="{y - 46:.1f}" width="18" height="92" rx="5" fill="none" stroke="{BLEU}" stroke-width="2"/>')
etiquette(x + 14, y - 4, ['broches', "de l'écran"], BLEU, 'start', 12)
txt(*v.p(56, Y1 + 6), 'VERSO : ESP32-C3 (+ QR code)', 16, 800, NUIT, 'middle')

# ---- 2. ESP32 au verso ----
titre(500, "2. L'ESP32-C3 au verso", 'à souder en premier, carte bien à plat')
k2 = 4.2
v = Vue('B', 35, 525, k2)
cx, cy = v.p(FX, FY)
esp(cx, cy, k2, broches=True)
ux, uy = v.p(FX, FY + 11.6)
fleche(ux + 150, uy + 30, ux + 4 * k2 + 4, uy - 6)
etiquette(ux + 150, uy + 40, ['USB-C en bas, sur le bord plat'])
x, y = v.p(FX - 9, FY - 6)
etiquette(x - 120, y - 30, ['puce et boutons', 'visibles (vers toi)'])
fleche(x - 50, y - 10, cx - 10, cy - 16, ROUGE, 2.2)
X = 585
txt(X, 555, 'Le bon sens, vu du verso :', 15, 800, NUIT)
for i, (ok, puce, bas, t1, t2) in enumerate(((True, True, True, 'puce visible', 'USB-C en bas, il dépasse du bord plat'),
                                             (False, False, True, 'retourné', 'puce contre la carte'),
                                             (False, True, False, 'tête en bas', 'USB-C vers le haut'))):
    y = 610 + i * 85
    esp(X + 35, y, 2.8, puce, bas)
    verdict(X + 90, y - 12, ok, t1)
    txt(X + 108, y + 12, t2, 12.5, 400, '#333')
puces(X, 862, ["Souder 1 coin, vérifier qu'il est droit,", 'puis les 15 autres demi-trous'], 13.5, 20)

# ---- 3. boutons puis ecran au recto ----
titre(915, '3. Au recto : les boutons, puis l\'écran', 'retourner la carte, QR code dessous')
r = Vue('F', 35, 940, k2)
for nom, bx, by, s in BOUTONS:
    r.rect(bx - s / 2, by - s / 2, s, s, rx=2, fill='#b9bfc8', stroke='#6d747d')
    x, y = r.p(bx, by)
    a(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{s * .31 * k2:.1f}" fill="{"#c4261b" if s == 12 else "#22252b"}"/>')
    txt(x, r.p(bx, by + s / 2)[1] + 16, nom, 13, 800, NUIT, 'middle')
r.rect(SX, SY, SW, SH, rx=5, fill='#1b2a5a', stroke='#0d1530')
r.rect(SX + 2.4, SY + 2.4, 54.5, SH - 4.8, rx=2, fill='#050607')
x, y = r.p(SX + 2.4 + 27, SY + SH / 2)
txt(x, y - 4, "dalle de l'écran", 15, 700, '#9fb3d9', 'middle')
txt(x, y + 16, 'vers toi', 15, 700, '#9fb3d9', 'middle')
x, y = r.p(J3X, J3Y)
a(f'<rect x="{x + 9:.1f}" y="{y - 9:.1f}" width="36" height="{7 * 2.54 * k2 + 18:.1f}" rx="4" fill="#fff" fill-opacity=".93" stroke="{NUIT}"/>')
for i, nom in enumerate(ECRAN):
    x, y = r.p(J3X, J3Y + 2.54 * i)
    a(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.6" fill="#d4ad4f" stroke="#7a5d1c"/>')
    txt(x + 27, y + 4, nom, 10.5, 800, ROUGE if nom == 'GND' else NUIT, 'middle')
x, y = r.p(J3X, SY)
etiquette(x + 10, y - 6, ['broches à droite'])
X = 585
txt(X, 970, "L'écran vu de côté :", 15, 800, NUIT)
bx, by = X + 20, 1045                                                       # profil : ecran, barrette, carte
a(f'<rect x="{bx}" y="{by - 42}" width="230" height="8" fill="#050607"/><rect x="{bx}" y="{by - 34}" width="250" height="6" fill="#1b2a5a"/>')
a(f'<rect x="{bx + 228}" y="{by - 28}" width="14" height="22" fill="#c4261b"/>')
for d in (0, 6):
    a(f'<rect x="{bx + 232 + d - 1.5}" y="{by - 30}" width="3" height="46" fill="#d4ad4f"/>')
a(f'<rect x="{bx - 10}" y="{by - 6}" width="300" height="8" fill="{VERT}"/>')
a(f'<path d="M{bx + 226},{by + 10} q9,-8 18,0 z" fill="#c9ced6" stroke="#777"/>')
txt(bx + 120, by - 50, 'dalle (recto, vers toi)', 12, 700, '#333', 'middle')
txt(bx + 120, by + 24, 'carte', 12, 700, VERT, 'middle')
txt(bx + 290, by + 30, 'soudure au verso', 12, 700, ROUGE, 'end')
puces(X, 1100, ['Boutons à plat, pattes à gauche et à droite', '2 petits à gauche, le gros (Saut) à droite',
                "Écran : les 8 broches traversent la carte", 'GND en haut, BL en bas (comme sur la carte)',
                "Le tenir parallèle : souder 1 broche, vérifier,", 'puis les 7 autres, côté verso'], 13.5, 20)

# ---- 4. verifier ----
titre(1275, '4. Vérifier avant de brancher')
puces(44, 1307, ["Aucun pont d'étain entre deux broches voisines (ESP et écran)",
                 "Au multimètre : pas de bip continu entre 3.3 et G (voir la fiche de test)",
                 "Brancher l'USB-C : l'écran titre s'affiche à l'endroit, Gauche / Droite / Saut répondent"], 14, 24)
txt(W / 2, H - 10, 'Dessin tiré du PCB pcb-polygon-ajoure-D - généré par make_fiche_assemblage.py', 11, 400, '#999', 'middle')
a('</svg>')

svg = '\n'.join(o)
svg = re.sub(r'id="(\w+)"', r'id="fa-\1"', svg)                         # ids uniques dans la page web
svg = re.sub(r'url\(#(\w+)\)', r'url(#fa-\1)', svg)
(ICI / 'fiche-assemblage-D.svg').write_text(svg, encoding='utf-8')
print('fiche-assemblage-D.svg ecrite', len(svg) // 1024, 'Ko')

page = ICI.parents[1] / 'brique' / 'index.html'
html = page.read_text(encoding='utf-8')
debut, fin = html.index('<!-- FICHE-ASSEMBLAGE'), html.index('<!-- /FICHE-ASSEMBLAGE -->')
debut = html.index('-->', debut) + 3
page.write_text(html[:debut] + svg + html[fin:], encoding='utf-8')
print('fiche mise a jour dans', page)
