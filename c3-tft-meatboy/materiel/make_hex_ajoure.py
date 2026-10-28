"""Genere polygon-hex-ajoure.svg : PCB nid d'abeilles d'un seul tenant (pas de pointilles),
ajoure par des trous hexagonaux, avec ecran TFT 2.0" et 3 boutons. Usage : python make_hex_ajoure.py"""
import math

R = 15.03; A = R * math.sqrt(3) / 2          # rayon / apotheme d'une alveole (mm)
ROW0 = [(56.68, 16.53), (84.29, 16.53)]
ROW1 = [(15.25, 38.97), (42.87, 38.97), (70.49, 38.97), (98.10, 38.97)]
ROW2 = [(29.06, 61.42), (56.68, 61.42), (84.29, 61.42)]
CELLS = ROW0 + ROW1 + ROW2


def hexpts(cx, cy, r):
    return " ".join(f"{cx + r * math.cos(math.radians(90 + 60 * k)):.2f},{cy - r * math.sin(math.radians(90 + 60 * k)):.2f}" for k in range(6))


RH = 7.6   # rayon des trous
HOLES = [(56.68, 12.2), (84.29, 12.2), (29.06, 65.8), (84.29, 65.8)]
MOUNT = [(46, 14.5), (95, 14.5), (47.5, 64), (65.9, 64)]
SX, SY, SW, SH = 25.68, 21.0, 62.0, 37.0       # module ecran (mm)
o = []
w = o.append


def rect(x, y, wd, h, **k):
    a = " ".join(f'{n.rstrip("_").replace("_", "-")}="{v}"' for n, v in k.items())
    w(f'<rect x="{x:.2f}" y="{y:.2f}" width="{wd:.2f}" height="{h:.2f}" {a}/>')


w('<svg xmlns="http://www.w3.org/2000/svg" width="100%" viewBox="-8 -8 128 92">')
w('''<defs>
<linearGradient id="pcb" gradientUnits="userSpaceOnUse" x1="0" y1="0" x2="112" y2="78"><stop offset="0" stop-color="#1f8a4c"/><stop offset=".55" stop-color="#136b3a"/><stop offset="1" stop-color="#0c5230"/></linearGradient>
<linearGradient id="gold" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#f3dc8e"/><stop offset=".5" stop-color="#c9a24a"/><stop offset="1" stop-color="#8f6d22"/></linearGradient>
<linearGradient id="metal" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#e6e9ee"/><stop offset=".5" stop-color="#a9b0b9"/><stop offset="1" stop-color="#6d747d"/></linearGradient>
<radialGradient id="capK" cx=".35" cy=".3" r=".8"><stop offset="0" stop-color="#5a5f68"/><stop offset="1" stop-color="#0e1013"/></radialGradient>
<radialGradient id="capR" cx=".35" cy=".3" r=".85"><stop offset="0" stop-color="#ff8f7d"/><stop offset=".5" stop-color="#d8261a"/><stop offset="1" stop-color="#7a0f0a"/></radialGradient>
<linearGradient id="sky" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#2a1f5c"/><stop offset="1" stop-color="#d9628a"/></linearGradient>
<linearGradient id="glare" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#fff" stop-opacity=".28"/><stop offset=".45" stop-color="#fff" stop-opacity=".04"/><stop offset=".46" stop-color="#fff" stop-opacity="0"/></linearGradient>
<filter id="sh" x="-10%" y="-10%" width="125%" height="130%"><feDropShadow dx=".8" dy="1.4" stdDeviation="1.1" flood-opacity=".45"/></filter>
<filter id="sh2" x="-30%" y="-30%" width="170%" height="170%"><feDropShadow dx=".25" dy=".45" stdDeviation=".3" flood-opacity=".6"/></filter>
<filter id="rim" x="-2%" y="-2%" width="104%" height="104%"><feMorphology in="SourceAlpha" operator="erode" radius=".45" result="e"/><feComposite in="SourceAlpha" in2="e" operator="out" result="r"/><feFlood flood-color="#7fd9a0" flood-opacity=".55"/><feComposite in2="r" operator="in" result="rr"/><feMerge><feMergeNode in="SourceGraphic"/><feMergeNode in="rr"/></feMerge></filter>
<mask id="body" maskUnits="userSpaceOnUse" x="-5" y="-5" width="125" height="90"><g fill="#fff" stroke="#fff" stroke-width=".3">''')
for c in CELLS:
    w(f'<polygon points="{hexpts(*c, R)}"/>')
w('</g><g fill="#000">')
for c in HOLES:
    w(f'<polygon points="{hexpts(*c, RH)}"/>')
for c in MOUNT:
    w(f'<circle cx="{c[0]}" cy="{c[1]}" r="1.7"/>')
w('</g></mask></defs>')
w('<rect x="-8" y="-8" width="128" height="92" fill="#d9dde3"/>')

# --- carte (un seul bloc, masque = alveoles - trous) ---
w('<g filter="url(#sh)"><g filter="url(#rim)"><g mask="url(#body)"><rect x="-5" y="-5" width="125" height="90" fill="url(#pcb)"/>')
# cuivre sous vernis : 8 pistes ecran -> bas, pistes boutons
tr = []
for i in range(8):
    py = 39.5 - 8.89 + i * 2.54
    bx = 93.2 - 0.7 * i; yh = 62.8 - 0.6 * i; px = 63.5 - 2.1 * i
    tr.append(f"M86.5,{py:.2f} H{bx:.2f} V{yh:.2f} H{px:.2f} V66")
for k, (x0, x1) in enumerate([(9.5, 41.0), (19.0, 42.2), (101.0, 43.4)]):
    y0 = 44 if x0 < 50 else 47
    tr.append(f"M{x0},{y0} V{56.2 + k * .8:.1f} H{x1} V64.6 H{49 + k * .0:.1f}" if x0 < 50 else f"M{x0},{y0} V56.6 H{x1}")
for d in tr:
    w(f'<path d="{d}" fill="none" stroke="#06200f" stroke-opacity=".5" stroke-width=".75" stroke-linejoin="round"/>')
    w(f'<path d="{d}" fill="none" stroke="#38b56d" stroke-opacity=".75" stroke-width=".42" stroke-linejoin="round"/>')
w('</g></g></g>')

# --- pastilles + silkscreen ---
for i in range(8):
    x = 63.5 - 2.1 * i
    w(f'<circle cx="{x:.2f}" cy="66" r=".85" fill="url(#gold)"/><circle cx="{x:.2f}" cy="66" r=".4" fill="#0b2a16"/>')
for (x, y) in MOUNT:
    w(f'<circle cx="{x}" cy="{y}" r="1.7" fill="#d9dde3"/><circle cx="{x}" cy="{y}" r="2.55" fill="none" stroke="url(#gold)" stroke-width=".7"/>')
    w(f'<circle cx="{x}" cy="{y}" r="1.7" fill="none" stroke="#06200f" stroke-opacity=".5" stroke-width=".25"/>')
S = 'fill="#f4f4ee" font-family="Arial,Helvetica,sans-serif" font-weight="700"'
w(f'<text x="56.68" y="71.2" {S} font-size="3.2" text-anchor="middle">MEAT RUN</text>')
w(f'<text x="56.68" y="74.2" {S} font-size="1.6" text-anchor="middle" opacity=".85">ESP32-C3 · v1</text>')
w('<rect x="45.7" y="60.5" width="22" height="11" rx=".6" fill="none" stroke="#f4f4ee" stroke-width=".25" stroke-dasharray="1 .7" opacity="0" />')

# --- ecran TFT (module 62 x 37) ---
w(f'<g filter="url(#sh2)"><rect x="{SX}" y="{SY}" width="{SW}" height="{SH}" rx="1.2" fill="#16181d"/>')
w(f'<rect x="{SX+.4}" y="{SY+.4}" width="{SW-.8}" height="{SH-.8}" rx=".9" fill="none" stroke="#2c3038" stroke-width=".3"/></g>')
# verre
gx, gy, gw, gh = SX + 2.4, SY + 2.4, 54.5, SH - 4.8
rect(gx, gy, gw, gh, rx=.4, fill="#050607", stroke="#3a3f48", stroke_width=".25")
ax, ay, aw, ah = gx + 6.7, gy + 1.7, 40.8, 30.6
w(f'<clipPath id="scr"><rect width="320" height="240"/></clipPath>')
w(f'<g clip-path="url(#scr)" transform="translate({ax:.2f} {ay:.2f}) scale({aw/320:.5f})">')
w('<rect width="320" height="240" fill="url(#sky)"/>')
w('<polygon points="0,170 50,95 95,150 140,80 200,165 250,110 320,170" fill="#4a2c6e" opacity=".85"/><polygon points="140,80 128,100 140,96 152,102" fill="#f2e9f7"/><polygon points="250,110 240,126 250,122 260,128" fill="#f2e9f7"/>')
w('<rect y="196" width="320" height="44" fill="#6b3b1f"/><rect y="196" width="320" height="9" fill="#3fbf4f"/>')
for x, y, wd in [(40, 160, 70), (140, 130, 60), (230, 100, 70)]:
    w(f'<rect x="{x}" y="{y}" width="{wd}" height="10" fill="#8b5a2b"/><rect x="{x}" y="{y}" width="{wd}" height="4" fill="#3fbf4f"/>')
w('<polygon points="272,96 296,78 296,96" fill="#ffd23f"/><rect x="293" y="62" width="3" height="34" fill="#ddd"/><polygon points="296,62 312,69 296,76" fill="#ffd23f"/>')
w('<rect x="58" y="136" width="24" height="24" fill="#d1121b"/><rect x="62" y="143" width="6" height="6" fill="#fff"/><rect x="72" y="143" width="6" height="6" fill="#fff"/><rect x="64" y="145" width="3" height="3" fill="#000"/><rect x="74" y="145" width="3" height="3" fill="#000"/>')
w('<circle cx="172" cy="176" r="20" fill="#c9ced6"/><circle cx="172" cy="176" r="6" fill="#555"/>')
w('<polygon points="170,100 174,111 186,111 176,118 180,129 170,122 160,129 164,118 154,111 166,111" fill="#ffd23f"/>')
w('<rect width="320" height="22" fill="#000" opacity=".55"/><text x="8" y="16" font-family="monospace" font-weight="700" font-size="14" fill="#fff">NIV 7   0:23</text><text x="312" y="16" font-family="monospace" font-weight="700" font-size="14" fill="#ffd23f" text-anchor="end">★★☆</text></g>')
w(f'<polygon points="{gx:.2f},{gy+gh:.2f} {gx+gw:.2f},{gy:.2f} {gx+gw:.2f},{gy+4:.2f} {gx+10:.2f},{gy+gh:.2f}" fill="url(#glare)" opacity=".8"/>')
w(f'<polygon points="{gx:.2f},{gy:.2f} {gx+gw*.55:.2f},{gy:.2f} {gx+gw*.3:.2f},{gy+gh:.2f} {gx:.2f},{gy+gh:.2f}" fill="url(#glare)" opacity=".9"/>')
# barrette 8 broches
hx = SX + SW - 3.4
rect(hx, 39.5 - 10.4, 2.6, 20.8, fill="#111", rx=.2)
for i in range(8):
    py = 39.5 - 8.89 + i * 2.54
    w(f'<circle cx="{hx+1.3:.2f}" cy="{py:.2f}" r=".65" fill="url(#gold)"/>')
w(f'<text x="{SX+SW-5.2:.2f}" y="{SY+2}" font-size="1.3" {S} text-anchor="end" opacity=".7">TFT 2.0"</text>')


def btn6(cx, cy, cap, label, arrow):
    s = 6.5
    rect(cx - s / 2 - .6, cy - s / 2 - .6, s + 1.2, s + 1.2, fill="none", stroke="#f4f4ee", stroke_width=".28")
    for dx in (-3.9, 3.9):
        for dy in (-2.25, 2.25):
            rect(cx + dx - .45, cy + dy - .35, .9, .7, fill="url(#gold)")
    w(f'<g filter="url(#sh2)"><rect x="{cx-3}" y="{cy-3}" width="6" height="6" rx=".4" fill="url(#metal)"/>')
    w(f'<circle cx="{cx}" cy="{cy}" r="1.9" fill="url(#{cap})"/><circle cx="{cx-.55}" cy="{cy-.6}" r=".5" fill="#fff" opacity=".35"/></g>')
    for sx, sy in [(-2.3, -2.3), (2.3, -2.3), (-2.3, 2.3), (2.3, 2.3)]:
        w(f'<circle cx="{cx+sx}" cy="{cy+sy}" r=".25" fill="#555"/>')
    w(f'<text x="{cx}" y="{cy+6.9}" {S} font-size="1.7" text-anchor="middle">{label}</text>')
    d = cx - 1.3 if arrow == "L" else cx + 1.3
    tip = cx - 2.1 if arrow == "L" else cx + 2.1
    w(f'<polygon points="{tip},{cy-5.2} {d},{cy-6.2} {d},{cy-4.2}" fill="#f4f4ee" transform="translate(0 0)"/>')


btn6(9.5, 38.5, "capK", "GAUCHE", "L")
btn6(19.0, 38.5, "capK", "DROITE", "R")
# bouton SAUT 12x12
cx, cy = 101.0, 38.5
rect(cx - 7.2, cy - 7.2, 14.4, 14.4, fill="none", stroke="#f4f4ee", stroke_width=".3")
for dx in (-6.9, 6.9):
    for dy in (-2.5, 2.5):
        rect(cx + dx - .8, cy + dy - .45, 1.6, .9, fill="url(#gold)")
w(f'<g filter="url(#sh2)"><rect x="{cx-6}" y="{cy-6}" width="12" height="12" rx=".7" fill="url(#metal)"/><rect x="{cx-5.2}" y="{cy-5.2}" width="10.4" height="10.4" rx=".5" fill="#2a2d33"/>')
w(f'<circle cx="{cx}" cy="{cy}" r="3.7" fill="url(#capR)" stroke="#5a0b07" stroke-width=".2"/><ellipse cx="{cx-1.2}" cy="{cy-1.5}" rx="1.3" ry=".9" fill="#fff" opacity=".4" transform="rotate(-35 {cx-1.2} {cy-1.5})"/></g>')
for sx in (-5, 5):
    for sy in (-5, 5):
        w(f'<circle cx="{cx+sx}" cy="{cy+sy}" r=".35" fill="#555"/>')
w(f'<text x="{cx}" y="{cy+10.2}" {S} font-size="2.3" text-anchor="middle">SAUT</text>')
w('</svg>')
open("polygon-hex-ajoure.svg", "w", encoding="utf-8").write("\n".join(o))
