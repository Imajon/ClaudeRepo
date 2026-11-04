# python envoyer-images.py [COM24] : jeu de test des images (b5) : un fond 160x120 en zoom 2 + un personnage 48x48 anime (8 images) qui traverse l'ecran
# Meme format que la page Brique (imgPaquet) : le script, l'octet 0, puis les images. Affiche OK/ERR puis les mesures STAT.
import math, struct, sys, time, serial

def image(l, h, frames, pal):                       # frames : liste de fonctions (x, y) -> indice de couleur 0..15
    bpl = (l + 1) // 2; px = bytearray(len(frames) * h * bpl)
    for f, fn in enumerate(frames):
        for y in range(h):
            for x in range(l):
                k = fn(x, y) & 15
                px[(f * h + y) * bpl + (x >> 1)] |= k << (0 if x & 1 else 4)
    return bytes([l, h, len(frames)]) + struct.pack('<16H', *pal) + bytes(px)

rgb = lambda r, g, b: (r >> 3) << 11 | (g >> 2) << 5 | b >> 3
ciel = [0] + [rgb(20 + i * 12, 40 + i * 10, 120 + i * 8) for i in range(15)]
fond = image(160, 120, [lambda x, y: 1 + min(14, y // 8) if (y < 90 or (x // 10 + y // 10) % 2) else 15], ciel)
perso = [0, rgb(255, 60, 40), rgb(255, 220, 0), rgb(255, 255, 255)] + [0] * 12
def pose(f):                                        # un disque rouge, des yeux, une jambe qui bouge
    return lambda x, y: (1 if (x - 24) ** 2 + (y - 20) ** 2 < 300 else 0) or (3 if (x - 30) ** 2 + (y - 16) ** 2 < 10 else 0) or \
                        (2 if 36 <= y < 48 and abs(x - 24 - 8 * math.sin(f * math.pi / 4)) < 3 else 0)
heros = image(48, 48, [pose(f) for f in range(8)], perso)
paquet = bytes([2]) + fond + heros

script = b"""# Test images
var x = 0
var sens = 1
def maj(dt)
  x += sens * 90 * dt
  if x > 272 sens = -1 end
  if x < 0 sens = 1 end
end
def dessine()
  image(1, 0, 0, 0, 2)
  image(2, int(x), 150, int(temps() * 8), 1, sens < 0)
  image(2, 20, 20, int(temps() * 4), 2)
  texte(4, 4, "images b5", rgb(255, 255, 255), 1)
end
"""
data = script + b"\0" + paquet
port = sys.argv[1] if len(sys.argv) > 1 else "COM24"
s = serial.Serial(port, 115200, timeout=0.1); time.sleep(1.5); s.reset_input_buffer()
s.write(b"VERSION\n"); time.sleep(0.3)
s.write(f"SCRIPT {len(data)} Test images\n".encode())
for i in range(0, len(data), 256): s.write(data[i:i + 256]); time.sleep(0.01)
time.sleep(1); s.write(b"STAT 1\n")
t = time.time()
while time.time() - t < 6:
    l = s.readline().decode(errors="replace").rstrip()
    if l: print(l)
s.write(b"STAT 0\n")
