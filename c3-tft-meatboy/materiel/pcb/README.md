# c3-tft-meatboy — PCB v8 "rectangle arrondi 110x55" (ecran reel, centre)

PCB support pour la console `c3-tft-meatboy` (ESP32-C3 SuperMini + ecran
TFT 2.0" 240x320 + 3 boutons GAUCHE/DROITE/SAUT), alimentation USB
uniquement. Contour **rectangle a coins arrondis** : **110 x 55mm**,
rayon 5mm (v8, 2026-09-28). Historique dans `versions/` : v6 "nuage"
113x84mm, v7 rectangle 110x71mm. Pour passer de 71 a 55mm de haut, le
**SuperMini (pastilles CMS au dos) a ete remonte de 15.74mm, derriere
l'ecran** (face opposee, pas de conflit) et ses pistes reroutees ; le bas
du module affleure toujours le bord bas (USB-C accessible). Boutons,
ecran et trous de fixation inchanges.

## Ce qui a change dans cette version
L'ecran reellement utilise (2.0" TFT, 240xRGBx320, module 37.10 x
62.00mm) est bien plus grand que l'hypothese initiale de 50mm. Le PCB a
donc ete repense entierement :
- **Ecran centre** sur la carte, monte **pivote a 90°** (paysage,
  broches sur le bord droit) pour correspondre a l'usage du jeu
  (rendu 320x240).
- **Contour agrandi** (8 bosses au lieu de 5) pour degager assez de
  place autour de l'ecran pour les boutons et les trous de fixation.
- **Trous de fixation (MTG1-4) repositionnes** pour s'aligner
  exactement sur les 4 trous de montage du module ecran (memes
  entraxes que sa fiche technique : 28.10mm entre trous adjacents cote
  broches, meme chose cote oppose) — une seule paire d'entretoises
  peut donc traverser a la fois le PCB et le module ecran.

## Disposition
- **Face avant** : ecran + les 3 boutons.
  - **Ecran TFT (J3)** : centre, broches sur le bord droit (2.54mm,
    8 broches), corps du module vers la gauche. Gabarit visible sur
    F.Fab pour verifier l'alignement avant de souder.
  - **DROITE / GAUCHE** : boutons-poussoirs **6x6mm standard** (pattern
    reel 6.50 x 4.50mm, trou Ø1.00mm), lobe gauche.
  - **SAUT** : bouton-poussoir **12x12mm standard** (pattern reel
    12.50 x 5.00mm, trou Ø1.30mm), lobe droit (pres de l'ecran).
- **Face arriere (dos)** : le controleur.
  - **ESP32-C3 SuperMini (J1/J2)** : soude en surface au dos
    (pastilles SMD sur B.Cu), USB-C accessible sur le bord bas.
    Silkscreen des broches imprime en miroir sur B.SilkS.

## Fichiers
- `c3-tft-meatboy.kicad_pcb` — projet KiCad 10. DRC : 0 erreur (2
  avertissements de serigraphie cosmetiques pres du contour, sans
  impact sur la fabrication).
- `gerbers/` — dossier pret a zipper et envoyer a un fabricant.

## Fournisseur suggere
- **JLCPCB** (jlcpcb.com) : upload direct du zip Gerber, 2 couches,
  FR4, 1.6mm, HASL. Verifie sur le configurateur que la taille detectee
  correspond a 110x55mm.
- Alternative : **PCBWay** (pcbway.com), memes reglages.

## AVANT de commander — a verifier absolument
1. **Ecran TFT** : dimensions reprises de la fiche technique fournie
   (37.10 x 62.00mm, broches 2.54mm x8, entraxe trous 28.00mm) —
   confirme que c'est bien le meme module que celui commande.
2. **SuperMini (SMD, dos)** : dimensions supposees 22.5 x 18mm, entraxe
   15.24mm/0.6" — a verifier au pied a coulisse. Montage en surface
   donc tolerant a un petit ecart (pattes pliables).
3. **Boutons 6x6mm / 12x12mm** : footprints repris de vraies fiches
   techniques (Sunrom), pas d'une estimation — ce sont les 2 tailles de
   boutons-poussoirs traversants les plus standard et faciles a
   trouver (Aliexpress, Mouser, Digikey...). Attention : ce ne sont
   **pas** juste une mise a l'echelle l'un de l'autre (6.50x4.50mm
   contre 12.50x5.00mm), donc si tu changes une taille pour une autre
   non listee ici, verifie la fiche technique exacte du bouton avant
   de commander.

Si une cote differe, ouvre `c3-tft-meatboy.kicad_pcb` dans KiCad et
ajuste les empreintes concernees avant de regenerer les Gerbers.

## Brochage (broche 1 = en haut sur le silkscreen)
- **J1** (rangee gauche SuperMini, SMD, dos) : GPIO0, GPIO1(DROITE), GPIO2(GAUCHE), GPIO3(SAUT), GPIO4(SCLK), 3V3, GND, 5V
- **J2** (rangee droite SuperMini, SMD, dos) : GPIO21, GPIO20, GPIO10(RST), GPIO9, GPIO8, GPIO7(CS), GPIO6(MOSI), GPIO5(DC)
- **J3** (ecran, traversant, face avant) : GND, VCC, SCL, SDA, RES, DC, CS, BLK
- USB-C du module SuperMini accessible au dos, bord bas.

## Assemblage
1. **Face avant** : souder les boutons SW1 (GAUCHE, 6x6mm), SW2
   (DROITE, 6x6mm), SW3 (SAUT, 12x12mm), puis enficher et souder le
   module ecran dans les 8 trous de J3 (verifier l'alignement avec le
   gabarit F.Fab).
2. **Retourner le PCB** : positionner le module ESP32-C3 SuperMini sur
   les pastilles SMD J1/J2 (gabarit B.Fab), **verifier l'orientation**
   (USB-C vers le bas) puis souder chaque patte a plat.
3. Fixer l'ecran et le PCB ensemble via les 4 trous alignes (MTG1-4),
   avec des entretoises.
4. Flasher via le port USB-C du module (accessible au dos, bord bas).
