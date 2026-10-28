"""Interroge Alto sur la console sans la redemarrer : envoie '?' et affiche images/s courantes, pire valeur de la course, temps de dessin.
Usage : python tools/alto_fps.py [COMxx]   (port detecte par find_esp32_port.py si absent)"""
import subprocess, sys, time
import serial

port = sys.argv[1] if len(sys.argv) > 1 else subprocess.run([sys.executable, 'find_esp32_port.py'], capture_output=True, text=True).stdout.strip().splitlines()[-1]
s = serial.Serial(); s.port = port; s.baudrate = 115200; s.timeout = 1
s.dtr = False; s.rts = False                     # lignes au repos avant l'ouverture : pas de redemarrage de la carte
s.open()
for _ in range(5):
    s.write(b'?'); time.sleep(1.2)
    print(s.read(s.in_waiting or 1).decode(errors='ignore').strip())
