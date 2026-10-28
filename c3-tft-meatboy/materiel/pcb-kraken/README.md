# c3-tft-meatboy — PCB Kraken (ESP32-C3 SuperMini)

Généré par `../make_kicad_polygon_c3.py kraken` (routeur `../route_tab.py`). Aperçu : `c3-tft-meatboy-kraken.svg`.

- **Contour** : `../Kraken.svg` à l'échelle 2,2 → **137 × 113 mm**, avec ses contreformes (spirales, croissants). Pas d'autres ajours.
- **Écran** : sur la tête et le corps ; il déborde un peu sur les creux tête/bras (voulu).
- **Boutons** : GAUCHE/DROITE sous le bras gauche, SAUT sous le bras droit.
- **C3 SuperMini** au dos, en bas au centre ; contour coupé à plat sous l'USB-C (fente de 13 mm) : la prise arrive au bord.
- **QR code** cuivre au dos, derrière la tête (miroir, se lit de derrière).
- **4 pastilles de fil** IO21/IO20 (bras gauche), IO0/IO8 (bras droit).
- **LED optionnelles** sur le haut de la tête (GAUCHE, DROITE, SAUT), CMS 1206 : +3V3 → R (≈1 kΩ) → LED → fil du bouton.
  Bouton appuyé = LED allumée, sans changer le code. Trait sérigraphié = cathode.
- GND par les plans de masse des 2 faces + vias de couture (pas de 6 mm).
- **DRC : 0 violation, 0 non connecté.** BL de l'écran (J3 broche 8) relié au +3V3.
- Versions précédentes : `versions/` (v1-sans-BL : BL de l'écran non relié).
