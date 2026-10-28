"""Avatar d'Alto : silhouette de assets/Snowborder2.png -> rider_sprite.h
Masque 1 bit stocke a 2x la taille d'affichage (RIDER_H px de haut a l'ecran) : le jeu tourne le sprite et moyenne 2x2 echantillons
par pixel d'ecran -> bords adoucis. Ancre = dessous de la planche, au milieu ; RIDER_ANG = pente de la planche dans l'image (rad).
Usage (depuis la racine du sketch) : python tools/tools_rider.py  -> rider_sprite.h + assets/_rider_apercu.png"""
import math
import numpy as np
from PIL import Image

RIDER_H = 38                       # hauteur a l'ecran (px)
S2 = 2                             # sur-echantillonnage stocke

im = Image.open('assets/Snowborder2.png').convert('RGBA')
im = im.crop(im.getbbox())
H = RIDER_H * S2
W = round(im.width * H / im.height)
a = np.array(im.resize((W, H), Image.LANCZOS))[:, :, 3].astype(int)
mask = a >= 128

# planche = tiers bas : droite des moindres carres, ancre sur sa face inferieure
yy, xx = np.where(mask)
lo = yy >= H * 0.68
bx, by = xx[lo].astype(float), yy[lo].astype(float)
cx, cy = bx.mean(), by.mean()
ang = math.atan2(((bx - cx) * (by - cy)).sum(), ((bx - cx) ** 2).sum())
nx, ny = -math.sin(ang), math.cos(ang)                     # normale vers le bas de la planche
under = max((x - cx) * nx + (y - cy) * ny for x, y in zip(bx, by))
ax, ay = cx + nx * under, cy + ny * under

with open('rider_sprite.h', 'w') as f:
    f.write('// genere par tools/tools_rider.py depuis assets/Snowborder2.png : silhouette 1 bit a 2x (%d px de haut a l ecran)\n' % RIDER_H)
    f.write('#pragma once\n#define RIDER_W2 %d\n#define RIDER_H2 %d\n' % (W, H))
    f.write('#define RIDER_AX2 %.2ff\n#define RIDER_AY2 %.2ff\n#define RIDER_ANG %.4ff\n' % (ax, ay, ang))
    # cou (accroche de l'echarpe) : arriere de la tete, juste sous le bas du crane
    hy = int(np.where(mask.any(axis=1))[0][0])
    nx2 = min(int(np.where(mask[y])[0].min()) for y in range(hy + int(H * 0.10), hy + int(H * 0.15))) + 1
    f.write('#define RIDER_NX2 %d.0f\n#define RIDER_NY2 %d.0f\n' % (nx2, hy + int(H * 0.18)))
    bits = []
    for y in range(H):
        for x0 in range(0, W, 8):
            v = 0
            for k in range(8):
                if x0 + k < W and mask[y, x0 + k]: v |= 0x80 >> k
            bits.append(v)
    f.write('#define RIDER_STRIDE %d\n' % ((W + 7) // 8))
    f.write('const uint8_t RIDER_BITS[%d] PROGMEM = {%s};\n' % (len(bits), ','.join(str(b) for b in bits)))

# apercu : sprite a la taille d'ecran, pose sur une pente de 20 degres (comme en jeu)
def render(theta, scale=1):
    out = np.zeros((70, 70))
    c, s = math.cos(theta), math.sin(theta)
    ox, oy = 35, 52
    for y in range(70):
        for x in range(70):
            cov = 0
            for sx_, sy_ in ((0.25, 0.25), (0.75, 0.25), (0.25, 0.75), (0.75, 0.75)):
                dx, dy = x + sx_ - ox, y + sy_ - oy
                u = ax + (dx * c + dy * s) * S2
                v = ay + (-dx * s + dy * c) * S2
                iu, iv = int(math.floor(u)), int(math.floor(v))
                if 0 <= iu < W and 0 <= iv < H and mask[iv, iu]: cov += 1
            out[y, x] = cov / 4
    return out
tiles = []
for slope_deg in (0, 20, 40):
    t = math.radians(slope_deg) - ang
    r = render(t)
    img = np.full((70, 70, 3), (236, 150, 172), np.uint8)
    # sol de la pente
    for x in range(70):
        gy = int(52 + (x - 35) * math.tan(math.radians(slope_deg)))
        img[max(0, gy):, x] = (118, 56, 80)
    img = (img * (1 - r[:, :, None]) + np.array((30, 14, 30)) * r[:, :, None]).astype(np.uint8)
    tiles.append(img)
sheet = np.concatenate(tiles, axis=1)
Image.fromarray(sheet).resize((sheet.shape[1] * 4, sheet.shape[0] * 4), Image.NEAREST).save('assets/_rider_apercu.png')
print('sprite %dx%d (2x) ancre (%.1f, %.1f) planche %.1f deg' % (W, H, ax, ay, math.degrees(ang)))
