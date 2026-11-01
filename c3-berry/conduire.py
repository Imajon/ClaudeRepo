# python conduire.py [COM23] : mesure le jeu charge pendant qu'on "conduit" (boutons simules par le cable)
import sys, time, serial
s = serial.Serial(sys.argv[1] if len(sys.argv) > 1 else "COM23", 115200, timeout=0.1)
time.sleep(2.5); s.reset_input_buffer(); s.write(b"STAT 1\n")
for nom, btn, d in (("arret", "000", 3), ("tout droit", "001", 4), ("virage", "011", 6), ("zigzag", None, 6)):
    print("---", nom); t = time.time(); z = 0
    if btn: s.write(f"BTN {btn}\n".encode())
    while time.time() - t < d:
        if not btn and time.time() - t > z * 0.6: s.write(f"BTN {'101' if z % 2 else '011'}\n".encode()); z += 1
        l = s.readline().decode(errors="replace").rstrip()
        if l.startswith("STAT"): print(l)
s.write(b"BTN 000\n")
