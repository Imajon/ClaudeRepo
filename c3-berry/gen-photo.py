# Image de demarrage : _images/C3-Brique/Selection Brique Web/MaconsoleBrique.jpeg -> photo.h (320x240, RGB565 deja dans l'ordre des octets de l'ecran)
# Relancer apres avoir change l'image : python gen-photo.py
import os
from PIL import Image

ICI = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(ICI, "..", "_images", "C3-Brique", "Selection Brique Web", "MaconsoleBrique.jpeg")
im = Image.open(SRC).convert("RGB")
im = im.crop((190, 0, 1250, im.height))      # le graffiti au centre, presque en 4:3 (fond noir autour)
h = round(320 * im.height / im.width)
fond = Image.new("RGB", (320, 240), im.getpixel((20, 20)))
fond.paste(im.resize((320, h), Image.LANCZOS), (0, (240 - h) // 2))
px = []
for r, g, b in fond.getdata():
    c = (r >> 3) << 11 | (g >> 2) << 5 | b >> 3
    px.append(((c & 255) << 8) | (c >> 8))       # octets inverses : copie directe dans la bande
with open(os.path.join(ICI, "photo.h"), "w") as f:
    f.write("// genere par gen-photo.py a partir de MaconsoleBrique.jpeg : ne pas modifier a la main\n#pragma once\n")
    f.write("const uint16_t PHOTO[320 * 240] = {\n")
    for i in range(0, len(px), 24):
        f.write(",".join("0x%04X" % v for v in px[i:i + 24]) + ",\n")
    f.write("};\n")
print("photo.h ecrit")
