# c3-tft-meatboy — PCB polygon ajouré B (ESP32-C3 SuperMini)

Généré par `../make_kicad_polygon_c3.py B` (routeur `../route_tab.py`). Aperçu : `c3-tft-meatboy-polygon-ajoure-B.svg`.

**= ajouré A + traits déco** : bandes de 2 mm à 45° (pas 4 mm) sur la partie haute (au-dessus de y = 31,5 mm, hors écran),
en **cuivre nu** (F.Cu + ouverture F.Mask, sans net). Coupées autour des pistes, pastilles, vias, trous et sérigraphie.

**+ languette d'accroche** détachable (façon Pocket Operator, comme l'ajouré C) : oblong 66 × 10,75 mm, trou 26 × 4 mm + demi-cercle R3,5
(crochet de rayon), 3 attaches de 2 mm sur les pointes du haut (pointes tronquées de 0,55 mm), bord cuivre nu + stries.

- **C3 SuperMini** au dos, en bas au centre ; pointe du bas coupée à plat (y = 73 mm) : l'USB-C arrive au bord de la carte.
- **QR code** cuivre au dos, **35,4 mm** (plaque 38 mm, de y = 10 à 48, au maximum entre le bord haut et le module) → https://studioeya.com (miroir, se lit de derrière). Pas d'étiquette (plus de place).
- **4 pastilles de fil** IO21/IO20 (gauche), IO0/IO8 (droite).
- **LED optionnelles** en haut (une par pointe : GAUCHE, DROITE, SAUT), CMS 1206 : +3V3 → R → LED → fil du bouton.
  Bouton appuyé = LED allumée, sans changer le code. Sans LED ni R, rien ne change.
  R ≈ 1 kΩ (LED rouge ~1,5 mA) ; descendre à 330 Ω pour plus de lumière. Pastille 1 de la LED (trait sérigraphié) = cathode.
- GND relié par les plans de masse des deux faces (pas de pistes GND).
- DRC : 0 non connecté ; 1 remarque (frein thermique de la broche GND de l'écran au dos, déjà reliée par la face avant). BL de l'écran (J3 broche 8) relié au +3V3.
- Versions précédentes : `versions/` (v1-qr-mods : QR 14 mm vers le GitHub ; v2-qr-studioeya : sans languette).
