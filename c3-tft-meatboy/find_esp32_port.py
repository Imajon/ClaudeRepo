#!/usr/bin/env python3
"""Detecte automatiquement le port COM d'une carte ESP32 branchee, pour eviter
de se tromper de port dans les .bat de compilation (l'USB CDC natif change
souvent de port a chaque flash). N'affiche rien si aucun port ESP32 unique
n'est trouve (le .bat retombe alors sur son port par defaut).
"""
import json
import subprocess
import sys

CLI = r"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"


def main():
    try:
        out = subprocess.run([CLI, "board", "list", "--discovery-timeout", "3s", "--format", "json"],
                              capture_output=True, text=True, timeout=25).stdout
        data = json.loads(out)
    except Exception as e:
        print(f"(detection auto du port impossible : {e})", file=sys.stderr)
        return
    candidates = []
    serial = []
    print("Ports detectes :", file=sys.stderr)
    for entry in data.get("detected_ports", []):
        boards = entry.get("matching_boards", [])
        addr = entry["port"]["address"]
        names = ", ".join(b.get("name", "?") for b in boards) or "(pas une carte reconnue)"
        print(f"  {addr:<8} {names}", file=sys.stderr)
        if entry["port"].get("protocol") == "serial": serial.append(addr)
        if any("esp32" in b.get("fqbn", "").lower() for b in boards):
            candidates.append(addr)
    if len(candidates) == 1:
        print(f"-> ESP32 detecte automatiquement sur {candidates[0]}", file=sys.stderr)
        print(candidates[0])
    elif not candidates and len(serial) == 1:      # un seul port serie branche : c'est forcement la carte
        print(f"-> un seul port serie disponible : {serial[0]}", file=sys.stderr)
        print(serial[0])
    elif len(candidates) > 1:
        print(f"-> plusieurs cartes ESP32 detectees ({', '.join(candidates)}) : choisis a la main.", file=sys.stderr)
    else:
        print("-> aucune carte ESP32 detectee : verifie le branchement, ou choisis le port a la main.", file=sys.stderr)


if __name__ == "__main__":
    main()
