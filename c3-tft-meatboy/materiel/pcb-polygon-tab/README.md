# c3-tft-meatboy — PCB « MA CONSOLE » (contour = textePCB.svg), routé

Géométrie identique à `../polygon-tab-lettres.svg`. Contour 110 x ~78 mm : lettres « MA CONSOLE » en haut, corps polygone.
Généré par `../make_kicad_tab.py` (+ routeur `../route_tab.py`), Gerbers exportés avec kicad-cli.

## Câblage (= .ino / ../montage-c3-tft.svg)
| Signal | GPIO C3 | Écran J3 / bouton |
|---|---|---|
| SCLK | 4 | SCL (broche 3) |
| MOSI | 6 | SDA (4) |
| CS | 7 | CS (7) |
| DC | 5 | DC (6) |
| RST | 10 | RES (5) |
| 3V3 | 3V3 | VCC (2) + BLK (8) |
| GND | GND | GND (1) + boutons |
| GAUCHE | 2 | SW1 (bouton le plus à gauche) |
| DROITE | 1 | SW2 |
| SAUT | 3 | SW3 (12x12) |
| IO21 / IO20 | 21 / 20 | pastilles de fil à gauche |
| IO0 / IO8 | 0 / 8 | pastilles de fil à droite |
GPIO9 (BOOT) et 5V ne sont pas utilisés.

## État
- **Routé** : 118 segments, 16 vias, 2 couches. **DRC : 0 violation, 0 non connecté** (`drc.rpt`).
- Cuivre des lettres (blanc du SVG) à 0,35 mm du bord ; règle cuivre/bord du projet = 0,25 mm (JLCPCB min 0,2).
- QR code cuivre 14 mm (URL à remplacer dans `make_polygon_ajoure.py`).

## À vérifier avant de commander
- Le SuperMini est posé au dos, **vu de devant sa rangée GPIO0…5V est à DROITE** (x=64,12), USB-C vers le bas. Vérifie sur ta carte réelle : pastilles 2,4 x 1,3 mm, entraxe des rangées 15,24 mm.
- Boutons 6x6 / 12x12 : empreintes simplifiées (pastilles Ø1,8 / Ø2,2) ; pattes d'un même côté reliées dans le bouton.
- Le routeur est maison (grille 0,2 mm) : relire le tracé dans KiCad avant fabrication.
