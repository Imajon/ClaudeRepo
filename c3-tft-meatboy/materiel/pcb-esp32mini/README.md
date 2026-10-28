# c3-tft-meatboy — PCB test "polygone" + ESP32 Mini NodeMCU (D1 Mini format)

Variante **test** du PCB de la console `c3-tft-meatboy`, demandee pour
essayer deux changements par rapport a la version "nuage" (`../pcb/`)
et a la version WROOM DevKitC (`../pcb-wroom/`) :

1. **Contour en polygone** (segments droits) au lieu d'arcs de cercle
   lisses — memes proportions/silhouette "nuage" que les versions
   precedentes (memes 8 cercles generateurs), juste echantillonnee en
   ~64 segments droits plutot qu'en arcs veritables.
2. **Controleur remplace par un "ESP32 Mini NodeMCU"** (format
   compact "D1 Mini ESP32", tres repandu chez les revendeurs), toujours
   monte au dos, a la place du SuperMini ou du DevKitC.

## ATTENTION — dimensions du module non verifiees sur fiche officielle
Contrairement au SuperMini et au DevKitC (dimensions Espressif
officielles verifiees dans les versions precedentes), le "ESP32 Mini
NodeMCU" est un clone generique vendu par de nombreux fabricants
**sans dimensions standardisees**. Cette empreinte suppose :
- 2 rangees de 8 broches, pas 2.54mm, **entraxe rangees 22.86mm
  (0.9", format "D1 Mini")**.
- Encombrement approx. 34 x 25mm (gabarit visible sur B.Fab).

**Avant de commander, mesure ton module reel au pied a coulisse**
(nombre de broches par rangee, entraxe, longueur/largeur) et compare
au gabarit B.Fab dans KiCad. Si ca ne correspond pas, les pastilles
J1/J2 devront etre repositionnees.

## Brochage suppose (a verifier selon le clone exact)
Le clone generique le plus courant reprend le silkscreen du D1 Mini
ESP8266 (TX, RX, D1-D8, 3V3, GND, 5V...) mais le mapping GPIO ESP32
sous-jacent varie d'un fabricant a l'autre. **Ce PCB utilise des
numeros de broches physiques (1 a 8 par rangee), pas des noms de
silkscreen** — il faudra identifier, sur le module reel, quelle broche
physique correspond a quel GPIO ESP32 (multimetre + datasheet du
vendeur) avant de souder, et adapter le firmware en consequence.

- **J1** (rangee gauche, cote boutons) : broche1=+3V3,
  broche2=GND, broche3=DROITE, broche4=GAUCHE, broches 5-8 non
  utilisees.
- **J2** (rangee droite, cote ecran/SAUT) : broche1=GND,
  broche2=+3V3, broche3=SCLK, broche4=SAUT, broche5=MOSI,
  broche6=RST, broche7=DC, broche8=CS.
- **J3** (ecran, traversant, face avant) : GND, VCC, SCLK, MOSI, RST,
  DC, CS, (broche8 = VCC redondante, **laissee non connectee** — la
  pastille 2 suffit a alimenter l'ecran).

## Disposition (identique aux versions precedentes)
- **Face avant** : ecran TFT 2.0" (J3, centre, pivote paysage) +
  boutons DROITE/GAUCHE (6x6mm standard) + SAUT (12x12mm standard).
- **Face arriere** : module ESP32 Mini NodeMCU, SMD (pastilles
  soudees a plat), silkscreen en miroir.
- 4 trous de fixation (MTG1-4), alignes sur le module ecran.

## Fichiers
- `c3-tft-meatboy-esp32mini.kicad_pcb` — projet KiCad 10.
  **DRC : 0 erreur, 0 element non connecte.**
- `gerbers/` + `c3-tft-meatboy-esp32mini-gerbers.zip` — prets a
  envoyer a un fabricant (JLCPCB, PCBWay...), 2 couches, FR4, 1.6mm.

## Notes de routage (pour repartir de ce fichier)
- Toutes les pistes sont en **B.Cu uniquement** (aucun via) : J1/J2
  sont des pastilles CMS natives au dos, et les pastilles
  boutons/ecran sont traversantes (accessibles depuis n'importe quelle
  face), donc aucun changement de couche n'est necessaire.
- Regle utilisee pour eviter tout croisement entre les ~10 signaux
  qui convergent vers J3/SW3 depuis J1/J2 : **l'ordre des broches
  source (J1/J2), l'ordre des cibles (y sur J3/SW3) et l'ordre des
  sorties laterales (x d'echappement) doivent tous croitre ensemble**
  (aucune inversion). C'est cette regle qui a ete cassee puis
  corrigee plusieurs fois pendant la conception — utile a savoir si
  tu deplaces un pad ou renumerotes un mapping.
- La pastille +3V3 redondante de J3 (broche 8) est volontairement
  laissee non connectee (voir ci-dessus) pour eviter un noeud de
  croisements pres de SW3.

## AVANT de commander
1. **Verifier le module ESP32 Mini reel** (voir avertissement
   ci-dessus) — c'est le point le moins fiable de cette variante test.
2. Boutons 6x6mm/12x12mm et ecran : memes fiches techniques verifiees
   que les versions precedentes (voir `../pcb/README.md`).
