#!/usr/bin/env python3
"""Convertit les bandes de decor (fonds de niveaux) en sprites indexes -> mountain_sprites.h
Sources (RGBA, fond transparent, se repetent sans raccord) :
  assets/_apercu_hill.png  -> HILL_ : collines vertes, niveaux 1-10 (exterieur)
  assets/_apercu_Glace.png -> MTN_  : roches/montagnes grises, niveaux 21-30 (glace)
  assets/_apercu_Lave.png  -> LAVA_ : roches rouges a lave, niveaux 11-20 (donjon de feu)
Usage : python tools_montagnes.py"""
import numpy as np
from PIL import Image

JOBS = [('HILL', 'assets/_apercu_hill.png', 64), ('MTN', 'assets/_apercu_Glace.png', 64), ('LAVA', 'assets/_apercu_Lave.png', 64)]


def rgb565(r, g, b): return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def resize_premult(rgb, alpha, w, h):
    """reduction ponderee par la transparence : pas de liseré sombre sur les bords"""
    a = alpha.astype(np.float32) / 255.0
    chans = []
    for c in range(3):
        p = Image.fromarray((rgb[:, :, c].astype(np.float32) * a)).resize((w, h), Image.BOX)
        chans.append(np.array(p))
    al = np.array(Image.fromarray(a).resize((w, h), Image.BOX))
    out = np.stack([np.where(al > 0.01, ch / np.maximum(al, 0.01), 0) for ch in chans], axis=2)
    return np.clip(out, 0, 255).astype(np.uint8), al >= 0.5


out = ['// Genere par tools_montagnes.py a partir de assets/*.png -- ne pas editer a la main.', '#pragma once', '#include <stdint.h>']
for name, path, target_h in JOBS:
    im = Image.open(path).convert('RGBA'); a = np.array(im)
    rows = np.where((a[:, :, 3] > 40).any(axis=1))[0]
    a = a[rows.min():rows.max() + 1]                                # coupe les marges transparentes
    w = round(a.shape[1] * target_h / a.shape[0])
    rgb, solid = resize_premult(a[:, :, :3], a[:, :, 3], w, target_h)
    q = Image.fromarray(rgb).quantize(colors=31, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()[:31 * 3]
    idx = np.array(q).astype(int) + 1
    idx[~solid] = 0
    pal565 = [0] + [rgb565(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]) for i in range(31)]
    out += [f'const uint16_t {name}_PAL[32] = {{ ' + ', '.join(f'0x{v:04X}' for v in pal565) + ' };', f'const uint8_t {name}_IDX[{w * target_h}] = {{']
    for r in range(target_h): out.append('  ' + ','.join(str(int(v)) for v in idx[r]) + ',')
    out += ['};', f'#define {name}_W {w}', f'#define {name}_H {target_h}']
    Image.fromarray(np.dstack([np.array(q.convert('RGB')), (solid * 255).astype(np.uint8)]), 'RGBA').resize((w * 3, target_h * 3), Image.NEAREST).save(f'assets/_test_{name.lower()}.png')
    print(name, path, '->', w, 'x', target_h)
open('mountain_sprites.h', 'w', encoding='utf-8').write('\n'.join(out) + '\n')
