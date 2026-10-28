#!/usr/bin/env python3
"""Planche des poses du bonhomme (copie de drawChibi du .ino) : stades 17-27 x poses, avec le contour du jeu.
Usage : python tools_poses.py -> poses-bonhomme.png"""
from PIL import Image, ImageDraw, ImageFont

WH = (246, 246, 250); SHD = (200, 204, 222); EY = (20, 20, 30); BL = (40, 100, 230); BLD = (20, 50, 140)
NAVY = (40, 46, 84); RIM = (225, 228, 238); GR = (160, 166, 182); GRD = (96, 102, 118)
RU = (176, 84, 44); RUL = (210, 120, 70); CY = (60, 236, 255); OR = (255, 150, 40); YE = (255, 220, 80); RD = (220, 60, 60)
KEY = (1, 2, 3)


def chibi(e, pose, fr, right=True, t=0.0):
    im = Image.new('RGB', (48, 56), KEY); g = ImageDraw.Draw(im); cx, by, k = 24, 50, 1; f = 1 if right else -1

    def R(x, y, w, h, c):
        if not right: x = -(x + w)
        g.rectangle([cx + x, by + y, cx + x + w - 1, by + y + h - 1], fill=c)
    body = GR if e >= 25 else WH if e >= 22 else BL
    hb = 1 if pose == 0 and int(t * 2.5) & 1 else 0; sq = 1 if pose == 4 else 0
    if pose == 5:
        R(-6, -7, 12, 3, body); R(-6, -12, 12, 6, WH)
        for (x, y) in ((-3, -10), (-2, -9), (1, -10), (2, -9)): R(x, y, 1, 1, EY)
        return im
    la, ra, ly, ry, lh, rh = -3, 1, -3, -3, 3, 3
    if pose == 1:
        if fr & 1: la, ly, lh, ra = -4, -4, 2, 2
        else: ra, ry, rh, la = 2, -4, 2, -4
    elif pose == 2: ly = ry = -4; lh = rh = 2; la, ra = -2, 0
    elif pose == 3: la, ra = -5, 3
    R(la, ly, 2, lh, SHD if body == WH else BLD); R(ra, ry, 2, rh, SHD if body == WH else BLD)
    if 22 <= e < 25: R(-8, -9, 3, 5, GR)
    if e == 23 and pose in (1, 2, 3): R(-8, -4, 3, 2 + (fr & 1), OR); R(-7, -2 + (fr & 1), 1, 2, YE)
    R(-4, -7 + sq, 8, 4 - sq, body)
    if e >= 22: R(-1, -6, 1, 1, RD); R(0, -6, 1, 1, CY); R(1, -6, 1, 1, YE)
    ay1 = ay2 = -7
    if pose == 1: ay1 = -7 - (fr & 1); ay2 = -7 - ((fr + 1) & 1)
    elif pose == 2: ay1 = ay2 = -11
    elif pose == 3: ay1 = ay2 = -12
    ax1, ax2 = (-7, 5) if pose == 3 else (-6, 4)
    R(ax1, ay1, 2, 3, body); R(ax2, ay2, 2, 3, body)
    if e >= 26: R(-7, -8, 3, 1, GRD); R(4, -8, 3, 1, GRD); R(5, -5, 5, 2, GRD); R(10, -5, 1, 2, CY)
    if e == 27 and pose in (2, 3): R(-3, 0, 2, 2 + (fr & 1), OR); R(1, 0, 2, 2 + (fr & 1), OR)
    hy = -17 + hb + sq; hx, hw = (-7, 14) if sq else (-6, 12)
    if e >= 24:
        R(-7, hy - 1, 14, 11, RU); R(-7, hy - 1, 14, 1, RUL); R(-5, hy + 1, 10, 6, (36, 30, 40))
        eh = 3 if pose == 2 else 2; R(-3, hy + 3, 2, eh, CY); R(1, hy + 3, 2, eh, CY)
        R(0, hy - 4, 1, 3, GRD); R(0, hy - 5, 1, 1, RD)
    else:
        x0 = cx + (hx if right else -(hx + hw)); g.rounded_rectangle([x0, by + hy, x0 + hw - 1, by + hy + 9], 2, fill=WH)
        R(hx + 1, hy + 8, hw - 2, 1, SHD)
        if e >= 21:
            g.rounded_rectangle([cx - 7, by + hy - 1, cx + 6, by + hy + 9], 4, fill=NAVY); g.rounded_rectangle([cx - 6, by + hy, cx + 5, by + hy + 8], 3, outline=RIM)
            R(-4, hy + 1, 1, 1, WH); R(-3, hy, 1, 1, WH); R(-1, hy + 5, 2, 2, BL); R(0, hy + 4, 1, 1, (150, 210, 255))
        else:
            eh = 3 if pose == 2 else 2; ew = 2 if pose == 2 else 1
            R(-1, hy + 4, ew, eh, EY); R(3, hy + 4, ew, eh, EY)
            if e >= 20: R(-4, hy + 4, 10, 2, EY); R(-3, hy + 4, 1, 1, (120, 120, 140))
            if e >= 19: R(-5, hy - 1, 11, 3, BL); R(4, hy + 1, 4, 1, BLD); R(-1, hy - 1, 1, 1, OR); R(0, hy - 1, 1, 1, YE)
            else:
                fh = 7 if e == 17 else 4
                g.polygon([(cx - 3, by + hy), (cx + 3, by + hy), (cx + f, by + hy - fh)], fill=BL)
                g.polygon([(cx - 1, by + hy), (cx + 1, by + hy), (cx + f, by + hy - fh + 3)], fill=(150, 210, 255))
    return im


def outlined(im, bg, oc):
    w, h = im.size; px = im.load(); out = Image.new('RGB', (w, h), bg); o = out.load()
    for y in range(h):
        for x in range(w):
            v = px[x, y]
            if v != KEY: o[x, y] = v
            elif any(0 <= x + dx < w and 0 <= y + dy < h and px[x + dx, y + dy] != KEY for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))): o[x, y] = oc
    return out


POSES = [(0, 0, 'repos'), (1, 0, 'course 1'), (1, 1, 'course 2'), (2, 0, 'saut'), (3, 0, 'chute'), (4, 0, 'atterrit'), (5, 0, 'perdu')]


def main():
    S = 5; cw, ch = 48 * S, 56 * S; gap = 4; lab = 26
    try: font = ImageFont.truetype('arial.ttf', 20)
    except OSError: font = ImageFont.load_default()
    stages = list(range(17, 28))
    W = 110 + len(POSES) * (cw + gap); H = 40 + len(stages) * (ch + gap)
    out = Image.new('RGB', (W, H), (14, 14, 22)); d = ImageDraw.Draw(out)
    for j, (_, _, n) in enumerate(POSES): d.text((110 + j * (cw + gap) + 8, 10), n, font=font, fill=(220, 220, 235))
    for i, e in enumerate(stages):
        y = 40 + i * (ch + gap); d.text((8, y + ch // 2), f"stade {e}", font=font, fill=(255, 220, 160))
        bg, oc = ((0, 0, 0), (210, 210, 230)) if i % 2 == 0 else ((130, 190, 235), (20, 20, 36))   # fond noir / ciel : contour adapte
        for j, (p, fr, _) in enumerate(POSES):
            out.paste(outlined(chibi(e, p, fr), bg, oc).resize((cw, ch), Image.NEAREST), (110 + j * (cw + gap), y))
    out.save('poses-bonhomme.png'); print('poses-bonhomme.png', out.size)


if __name__ == '__main__':
    main()
