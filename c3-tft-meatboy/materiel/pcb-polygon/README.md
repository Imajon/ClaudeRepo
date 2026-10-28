# c3-tft-meatboy — PCB test "polygon.svg" + ESP32-C3 SuperMini

Variante **test** du PCB `c3-tft-meatboy`, dont le contour reprend
**exactement** le polygone dessine dans `../polygon.svg` (le fichier
SVG present a la racine du projet, cree sous Inkscape) — un contour
en "engrenage" a 22 sommets, 3 dents en haut, 3 en bas, et une pointe
de chaque cote — plutot que le contour "nuage" (arcs de cercle) des
versions precedentes.

## Origine du contour
Le chemin `path236` de `polygon.svg` a ete extrait tel quel (22
sommets, segments droits/verticaux uniquement — c'etait deja un
polygone dans le SVG source, pas des courbes) puis mis a l'echelle et
translate. Aucune circle-union ni lissage n'a ete applique : le
contour Edge.Cuts est litteralement les 22 segments du SVG, a
l'echelle.

**Format final : 110mm de large** (bbox source 164.84 x 112.88mm ->
echelle x0.6674 -> **~110 x 75.5mm**, memes proportions exactes que le
SVG). Reduit depuis la version initiale (142mm) a la demande, avec les
boutons rapproches de l'ecran (marge ecran->boutons reduite de ~9mm a
~2.5mm) pour compenser la perte de hauteur disponible.

## Contrôleur : ESP32-C3 SuperMini (2026-10-15)
La v1 avait l'empreinte d'un ESP32 D1 Mini (rangées à 22,86 mm d'entraxe, ~24,9 mm bord à bord) :
trop large pour le C3. Remplacée par celle du SuperMini (CMS au dos, rangées à **15,24 mm**
d'entraxe, pastilles 2,4 x 1,3 mm → 17,6 mm bord à bord), même brochage que le .ino et
`../pcb-polygon-tab/` (SCLK=4, MOSI=6, CS=7, DC=5, RST=10, GAUCHE=2, DROITE=1, SAUT=3).
Vu de devant, la rangée GPIO0…5V est à droite, USB-C vers le bas. Reroutage automatique
(`../make_kicad_polygon_c3.py` + `../route_tab.py`), **DRC : 0 violation, 0 non connecté**.
Ancienne version : `versions/c3-tft-meatboy-polygon-v1-esp32mini.*`.

## Disposition (adaptee a la forme "engrenage")
La silhouette a un rectangle central flanque de dents triangulaires
(haut/bas) et de pointes (gauche/droite) — tous les composants sont
places DANS le rectangle central, pour rester loin des dents/pointes :
- **Ecran TFT (J3)** : en haut a droite du rectangle central.
- **DROITE / GAUCHE (6x6mm)** : juste en dessous de l'ecran, cote a
  cote a gauche (rapproches au maximum : ~2.5mm de marge).
- **SAUT (12x12mm)** : a la meme hauteur, a droite — **face au groupe
  GAUCHE/DROITE** comme demande.
- **Module ESP32 Mini (dos)** : centre, entre les boutons et le bas
  du contour.
- **4 trous de fixation** : dans les coins libres de part et d'autre
  de l'ecran (les seules zones vraiment degagees de ce contour).

## Fichiers
- `c3-tft-meatboy-polygon.kicad_pcb` — projet KiCad 10.
  **DRC : 0 erreur, 0 element non connecte.**
- `gerbers/` + `c3-tft-meatboy-polygon-gerbers.zip` — prets a envoyer
  a un fabricant (JLCPCB, PCBWay...), 2 couches, FR4, 1.6mm.

## AVANT de commander
1. **SuperMini** : vérifie l'entraxe des rangées (15,24 mm) au pied à coulisse.
2. Boutons 6x6mm/12x12mm et ecran : memes fiches techniques verifiees
   que les versions precedentes (voir `../pcb/README.md`).
3. Le contour "engrenage" a des pointes assez fines (dents ~10mm de
   base) — verifie aupres du fabricant que sa tolerance de decoupe
   (routage mecanique) peut reproduire des angles aussi marques sans
   ebrechure.
