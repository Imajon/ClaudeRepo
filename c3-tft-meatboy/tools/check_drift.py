"""Verifie les circuits de drift.ino hors carte : meme lissage Catmull-Rom que drBuild, ecart minimal entre portions non voisines
(il faut > largeur + 2 x marge pour que les murs ne se touchent pas) et image assets/_drift_circuits.png.
Usage : python tools/check_drift.py"""
import re, math
from PIL import Image, ImageDraw
src = open('drift.ino', encoding='utf-8').read()
blk = src[src.index('DR_TR[3] = {'):src.index('};', src.index('DR_TR[3] = {'))]
tracks = []
for m in re.finditer(r'\{ "(\w+)", (\d+), \{[^}]*\}, (\d+),\s*\{(.*?)\} \}', blk, re.S):
    pts = [tuple(map(int, p)) for p in re.findall(r'\{ (-?\d+), (-?\d+)', m.group(4))]
    assert len(pts) == int(m.group(3)), m.group(1)
    tracks.append((m.group(1), int(m.group(2)), pts))
SUB = 6
ok = True
img = Image.new('RGB', (3 * 330, 260), (14, 14, 24)); d = ImageDraw.Draw(img)
for ti, (name, w, P) in enumerate(tracks):
    c = []
    for i in range(len(P) - 1):
        p0, p1, p2, p3 = P[max(0, i - 1)], P[i], P[i + 1], P[min(len(P) - 1, i + 2)]
        for s in range(SUB):
            t = s / SUB; t2, t3 = t * t, t * t * t
            b = (-0.5 * t3 + t2 - 0.5 * t, 1.5 * t3 - 2.5 * t2 + 1, -1.5 * t3 + 2 * t2 + 0.5 * t, 0.5 * t3 - 0.5 * t2)
            c.append((sum(b[k] * q[0] for k, q in enumerate((p0, p1, p2, p3))), sum(b[k] * q[1] for k, q in enumerate((p0, p1, p2, p3)))))
    c.append(P[-1])
    n = len(c); L = 0
    for i in range(n - 1): L += math.dist(c[i], c[i + 1])
    # ecart minimal entre points de la ligne centrale eloignes d'au moins 2 x largeur le long du trace
    arc = [0]
    for i in range(n - 1): arc.append(arc[-1] + math.dist(c[i], c[i + 1]))
    mind, where = 1e9, None
    for i in range(n):
        for j in range(i + 1, n):
            if arc[j] - arc[i] > 2.2 * w:
                dd = math.dist(c[i], c[j])
                if dd < mind: mind, where = dd, (i, j)
    # rayon de courbure minimal (le bord interieur se croise si rayon < w/2)
    rmin = 1e9
    for i in range(1, n - 1):
        a, b_, cc = c[i - 1], c[i], c[i + 1]
        A = math.dist(a, b_); B = math.dist(b_, cc); C = math.dist(a, cc)
        area = abs((b_[0] - a[0]) * (cc[1] - a[1]) - (b_[1] - a[1]) * (cc[0] - a[0])) / 2
        if area > 1e-6: rmin = min(rmin, A * B * C / (4 * area))
    good = mind > w + 14 and rmin > w / 2 + 2 and n <= 200
    ok &= good
    print('%-9s points=%3d longueur=%5.0f px  ecart mini=%5.1f (largeur %d)  rayon mini=%5.1f  %s' % (name, n, L, mind, w, rmin, 'OK' if good else 'A REVOIR %s' % (where,)))
    xs = [p[0] for p in c]; ys = [p[1] for p in c]; sc = min(300 / (max(xs) - min(xs) + w), 230 / (max(ys) - min(ys) + w))
    ox, oy = ti * 330 + 15, 15
    T = lambda p: (ox + (p[0] - min(xs) + w / 2) * sc, oy + (p[1] - min(ys) + w / 2) * sc)
    for i in range(n - 1):
        (x0, y0), (x1, y1) = c[i], c[i + 1]; tx, ty = x1 - x0, y1 - y0; l = math.hypot(tx, ty) or 1
        nx, ny = -ty / l * w / 2, tx / l * w / 2
        d.line([T((x0 + nx, y0 + ny)), T((x1 + nx, y1 + ny))], fill=(80, 230, 255)); d.line([T((x0 - nx, y0 - ny)), T((x1 - nx, y1 - ny))], fill=(80, 230, 255))
    d.ellipse([T(c[0])[0] - 3, T(c[0])[1] - 3, T(c[0])[0] + 3, T(c[0])[1] + 3], fill=(255, 255, 255))
    d.ellipse([T(c[-1])[0] - 3, T(c[-1])[1] - 3, T(c[-1])[0] + 3, T(c[-1])[1] + 3], fill=(255, 60, 60))
    d.text((ox, 240), name, fill=(220, 220, 240))
img.save('assets/_drift_circuits.png')
assert ok, 'circuit a revoir'
