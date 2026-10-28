"""Genere proto-0-4/proto-0-4.ino : copie du sketch principal avec l'ecran sur GPIO0-4 (SCL,SDA,RES,DC,CS dans l'ordre)
et les boutons sur GPIO10,21,5. Usage : python make_proto_ino.py  (relancer apres chaque modif du .ino principal)"""
import glob, os, re, shutil

PINS = {"TFT_SCLK": 0, "TFT_MOSI": 1, "TFT_RST": 2, "TFT_DC": 3, "TFT_CS": 4, "BTN_LEFT": 10, "BTN_RIGHT": 21, "BTN_JUMP": 5}
s = open("c3-tft-meatboy.ino", encoding="utf-8").read()
for k, v in PINS.items():
    s, n = re.subn(rf"(#define {k}\s+)\d+", rf"\g<1>{v}", s, count=1)
    assert n == 1, k
s, n = re.subn(r'(#define FW_VERSION "[^"]*?)"', r'\1P"', s, count=1)
assert n == 1
s = s.replace("/*\n * c3-tft-meatboy", """/*
 * VARIANTE PROTO (genere par make_proto_ino.py - ne pas editer ici) : ecran SCL,SDA,RES,DC,CS -> GPIO0,1,2,3,4 ; BLK -> 3V3 ;
 * boutons GAUCHE,DROITE,SAUT -> GPIO10,21,5 (autre borne -> GND). Le reste = sketch principal, commentaire d'origine ci-dessous.
 *
 * c3-tft-meatboy""", 1)
os.makedirs("proto-0-4", exist_ok=True)
open("proto-0-4/proto-0-4.ino", "w", encoding="utf-8", newline="").write(s)
for t in glob.glob("*.ino"):                      # autres onglets du sketch (alto.ino...)
    if t != "c3-tft-meatboy.ino":
        shutil.copy(t, "proto-0-4")
for h in glob.glob("*.h"):
    shutil.copy(h, "proto-0-4")
print("proto-0-4/proto-0-4.ino genere")
