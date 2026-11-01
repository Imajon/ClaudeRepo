# python envoyer.py jeux/drift.be [COM23] [secondes] : envoie un script Berry a la console, puis affiche ses reponses (OK / ERR / STAT / print)
import sys, time, serial

fichier = sys.argv[1]
port = sys.argv[2] if len(sys.argv) > 2 else "COM23"
duree = float(sys.argv[3]) if len(sys.argv) > 3 else 6
data = open(fichier, "rb").read()
s = serial.Serial(port, 115200, timeout=0.1)
time.sleep(1.5); s.reset_input_buffer(); s.write(b"STAT 1\n")
s.write(f"SCRIPT {len(data)}\n".encode())
for i in range(0, len(data), 256):                  # par morceaux : le tampon USB de la console est petit
    s.write(data[i:i + 256]); time.sleep(0.01)
t = time.time()
while time.time() - t < duree:
    l = s.readline().decode(errors="replace").rstrip()
    if l: print(l)
