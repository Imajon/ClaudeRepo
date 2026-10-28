# c3-tft-meatboy — PCB polygon ajouré A (ESP32-C3 SuperMini)

Généré par `../make_kicad_polygon_c3.py A` (routeur `../route_tab.py`). Aperçu : `c3-tft-meatboy-polygon-ajoure-A.svg`.

- **C3 SuperMini** au dos, en bas au centre ; pointe du bas coupée à plat (y = 73 mm) : l'USB-C arrive au bord de la carte.
- **QR code** cuivre au dos, derrière l'écran (miroir, se lit de derrière).
- **4 pastilles de fil** IO21/IO20 (gauche), IO0/IO8 (droite).
- **LED optionnelles** en haut (une par pointe : GAUCHE, DROITE, SAUT), CMS 1206 : +3V3 → R → LED → fil du bouton.
  Bouton appuyé = LED allumée, sans changer le code. Sans LED ni R, rien ne change.
  R ≈ 1 kΩ (LED rouge ~1,5 mA) ; descendre à 330 Ω pour plus de lumière. Pastille 1 de la LED (trait sérigraphié) = cathode.
- GND relié par les plans de masse des deux faces (pas de pistes GND).
- DRC : 0 violation, 0 non connecté. BL de l'écran (J3 broche 8) relié au +3V3.
- Versions précédentes : `versions/` (v2-sans-BL : BL de l'écran non relié).
