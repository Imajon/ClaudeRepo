#!/usr/bin/env python3
"""Planche de proposition : evolution de l'avatar APRES la flamme (bonhomme -> cosmonaute -> robot), avant de coder.
Style de la planche de reference : petit bonhomme blanc a grosse tete, contour sombre, accessoires.
Dessins a l'echelle du jeu (1 px = 1 px ecran), agrandis x6. Usage : python tools_evolution_suite.py -> evolution-suite.png"""
import math
from PIL import Image, ImageDraw, ImageFont
from tools_evolution import stage, flame_shape, O, M, C, CW, CH, S, BG, PANEL

OUT = (28, 28, 48); WH = (246, 246, 250); SHD = (200, 204, 222); EYE = (20, 20, 30)
BLUE = (40, 100, 230); BLUED = (20, 50, 140); ORG = (255, 150, 40); YEL = (255, 220, 80)
NAVY = (40, 46, 84); RIM = (225, 228, 238); GRY = (160, 166, 182); GRYD = (96, 102, 118)
RUST = (176, 84, 44); RUSTD = (110, 50, 30); CYAN = (60, 236, 255); RED = (220, 60, 60)

BODY = [                                   # 16 x 17 : tete (0-9), corps (10-16)
    "....########....",
    "...#wwwwwwww#...",
    "..#wwwwwwwwww#..",
    ".#wwwwwwwwwwww#.",
    ".#wwwwwwwwwwww#.",
    ".#wwwwwewwewww#.",
    ".#wwwwwewwewww#.",
    ".#wwwwwwwwwwww#.",
    ".#swwwwwwwwwws#.",
    "..#ssssssssss#..",
    "...##bbbbbb##...",
    "..#b#bbbbbb#b#..",
    "..#b#bbbbbb#b#..",
    "...##bbbbbb##...",
    "....#bb##bb#....",
    "....#bb##bb#....",
    "....###..###....",
]


def chibi(d, ox, oy, body=WH, bodys=None, face=True):
    pal = {'#': OUT, 'w': WH, 's': SHD, 'e': EYE if face else WH, 'b': body}
    for r, row in enumerate(BODY):
        for c, ch in enumerate(row):
            if ch != '.': d.point((ox + c, oy + r), fill=pal[ch])


def rect(d, x0, y0, x1, y1, c, out=None):
    d.rectangle([x0, y0, x1, y1], fill=c)
    if out: d.rectangle([x0, y0, x1, y1], outline=out)


def base():
    im = Image.new('RGB', (CW, CH), (158, 164, 196)); d = ImageDraw.Draw(im)   # fond lavande (comme la reference) : le contour sombre se voit
    by = CH - 6; d.line([(2, by + 1), (CW - 3, by + 1)], fill=(120, 126, 160))
    ox, oy = CW // 2 - 8, by - 16
    return im, d, ox, oy, by


def cap(d, ox, oy):                         # casquette bleue, logo flamme
    rect(d, ox + 2, oy - 1, ox + 13, oy + 2, BLUE, OUT); rect(d, ox + 12, oy + 2, ox + 16, oy + 3, BLUED, OUT)
    d.point((ox + 7, oy + 0), fill=ORG); d.point((ox + 8, oy + 0), fill=YEL)


def jet_flame(d, x, y, big=False):
    d.polygon([(x - 2, y), (x + 2, y), (x, y + (6 if big else 4))], fill=ORG); d.line([(x, y + 1), (x, y + (3 if big else 2))], fill=YEL)


def s17():                                  # bonhomme flamme : la flamme devient une meche
    im, d, ox, oy, by = base(); chibi(d, ox, oy, BLUE)
    flame_shape(d, ox + 8, oy + 1, 10, 3.5, 1.5, O, M, C)
    d.point((ox + 3, oy + 7), fill=(150, 190, 255)); d.point((ox + 12, oy + 7), fill=(150, 190, 255))
    return im


def s18():                                  # bonhomme
    im, d, ox, oy, by = base(); chibi(d, ox, oy, BLUE)
    flame_shape(d, ox + 8, oy + 1, 5, 2, 1, O, M, C)
    return im


def s19():                                  # casquette
    im, d, ox, oy, by = base(); chibi(d, ox, oy, BLUE); cap(d, ox, oy); return im


def s20():                                  # casquette + lunettes
    im, d, ox, oy, by = base(); chibi(d, ox, oy, BLUE); cap(d, ox, oy)
    rect(d, ox + 4, oy + 5, ox + 11, oy + 6, EYE); d.point((ox + 5, oy + 5), fill=(120, 120, 140))
    return im


def helmet(d, ox, oy):                      # casque de cosmonaute (comme la reference) : bulle sombre, liseré clair, reflet
    d.ellipse([ox - 1, oy - 2, ox + 16, oy + 11], fill=NAVY, outline=OUT)
    d.ellipse([ox, oy - 1, ox + 15, oy + 10], outline=RIM)
    d.point((ox + 4, oy + 2), fill=(255, 255, 255)); d.point((ox + 5, oy + 1), fill=(255, 255, 255)); d.point((ox + 3, oy + 3), fill=(200, 210, 255))
    flame_shape(d, ox + 8, oy + 8, 4, 1.6, 0, O, M, C)     # la flamme se devine dans le casque


def s21():
    im, d, ox, oy, by = base(); chibi(d, ox, oy, WH, face=False); helmet(d, ox, oy); return im


def suit(d, ox, oy):
    rect(d, ox - 2, oy + 10, ox + 2, oy + 15, GRY, OUT)                   # sac a dos
    chibi(d, ox, oy, WH, face=False)
    d.point((ox + 7, oy + 11), fill=RED); d.point((ox + 8, oy + 11), fill=CYAN); d.point((ox + 9, oy + 11), fill=YEL)   # panneau de poitrine
    helmet(d, ox, oy)


def s22():
    im, d, ox, oy, by = base(); suit(d, ox, oy); return im


def s23():
    im, d, ox, oy, by = base(); suit(d, ox, oy); jet_flame(d, ox, oy + 16, True); return im


def robot_head(d, ox, oy, tv=False):        # tete de robot rouille, visiere sombre, yeux cyan (reference)
    rect(d, ox, oy - 1, ox + 15, oy + 10, RUST, OUT); rect(d, ox + 1, oy, ox + 14, oy, (210, 120, 70))
    rect(d, ox + 3, oy + 2, ox + 12, oy + 7, (36, 30, 40), OUT)
    rect(d, ox + 5, oy + 4, ox + 6, oy + 5, CYAN); rect(d, ox + 9, oy + 4, ox + 10, oy + 5, CYAN)
    d.line([(ox + 8, oy - 2), (ox + 8, oy - 4)], fill=OUT); d.point((ox + 8, oy - 5), fill=RED)


def s24():
    im, d, ox, oy, by = base(); chibi(d, ox, oy, WH, face=False); robot_head(d, ox, oy); return im


def s25():
    im, d, ox, oy, by = base(); chibi(d, ox, oy, GRY, face=False); robot_head(d, ox, oy)
    d.point((ox + 7, oy + 11), fill=CYAN); d.point((ox + 8, oy + 11), fill=CYAN); rect(d, ox + 5, oy + 13, ox + 10, oy + 13, GRYD)
    return im


def s26():                                  # robot : epaulettes + petit canon
    im, d, ox, oy, by = base(); chibi(d, ox, oy, GRY, face=False); robot_head(d, ox, oy)
    rect(d, ox + 1, oy + 10, ox + 3, oy + 11, GRYD, OUT); rect(d, ox + 12, oy + 10, ox + 14, oy + 11, GRYD, OUT)
    rect(d, ox + 13, oy + 12, ox + 18, oy + 13, GRYD, OUT); d.point((ox + 19, oy + 12), fill=CYAN); d.point((ox + 19, oy + 13), fill=CYAN)
    d.point((ox + 7, oy + 11), fill=CYAN); d.point((ox + 8, oy + 11), fill=CYAN)
    return im


def s27():                                  # robot final : propulseurs sous les pieds + halo
    im, d, ox, oy, by = base()
    for r, c in ((20, (120, 140, 220)), (17, (140, 170, 240))): d.ellipse([ox + 8 - r, oy + 6 - r, ox + 8 + r, oy + 6 + r], outline=c)
    oy -= 4
    chibi(d, ox, oy, GRY, face=False); robot_head(d, ox, oy)
    rect(d, ox + 1, oy + 10, ox + 3, oy + 11, GRYD, OUT); rect(d, ox + 12, oy + 10, ox + 14, oy + 11, GRYD, OUT)
    rect(d, ox + 13, oy + 12, ox + 18, oy + 13, GRYD, OUT); d.point((ox + 19, oy + 12), fill=CYAN)
    jet_flame(d, ox + 6, oy + 17); jet_flame(d, ox + 10, oy + 17)
    return im


NEW = [(s17, '17. meche de flamme'), (s18, '18. bonhomme'), (s19, '19. casquette'), (s20, '20. lunettes'),
       (s21, '21. casque cosmo'), (s22, '22. combinaison'), (s23, '23. jetpack'),
       (s24, '24. tete robot'), (s25, '25. corps metal'), (s26, '26. canon'), (s27, '27. robot final')]


def main():
    cell = (CW * S, CH * S); gap = 4; lab = 30; cols = 6
    try: font = ImageFont.truetype('arial.ttf', 22); big = ImageFont.truetype('arialbd.ttf', 30)
    except OSError: font = big = ImageFont.load_default()
    recap = [0, 3, 7, 9, 11, 16]
    Wd = cols * (cell[0] + gap) + gap
    Hd = 60 + (1 + math.ceil(len(NEW) / cols)) * (cell[1] + lab + gap) + 60
    out = Image.new('RGB', (Wd, Hd), BG); d = ImageDraw.Draw(out)
    d.text((gap + 6, 14), "Deja en jeu (rappel) : pixels -> couleurs -> trainee -> flamme", font=big, fill=(200, 200, 215))
    y = 60
    for i, e in enumerate(recap):
        x = gap + i * (cell[0] + gap); out.paste(stage(e).resize(cell, Image.NEAREST), (x, y))
        d.text((x + 6, y + cell[1] + 2), f"{e}.", font=font, fill=(160, 160, 180))
    y += cell[1] + lab + gap + 6
    d.text((gap + 6, y), "Proposition apres la flamme : bonhomme -> cosmonaute -> robot", font=big, fill=(255, 210, 120))
    y += 44
    for i, (fn, name) in enumerate(NEW):
        r, c = divmod(i, cols); x = gap + c * (cell[0] + gap); yy = y + r * (cell[1] + lab + gap)
        out.paste(fn().resize(cell, Image.NEAREST), (x, yy))
        d.text((x + 6, yy + cell[1] + 2), name, font=font, fill=(255, 220, 160))
    out = out.crop((0, 0, Wd, y + math.ceil(len(NEW) / cols) * (cell[1] + lab + gap) + 6))
    out.save('evolution-suite.png'); print('evolution-suite.png', out.size)


if __name__ == '__main__':
    main()
