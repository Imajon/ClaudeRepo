# c3-tft-meatboy — PCB "polygon.svg" + ESP32 DevKit V1 30 broches (ELEGOO) + HP + prototypage

Variante du PCB `c3-tft-meatboy` (contour exact de `../polygon.svg`,
110 x 75.5mm, ecran + boutons identiques a `../pcb-polygon/`), avec
trois changements :

1. **Controleur ESP32 "DevKit V1" 30 broches**, verifie correspondre
   au format de la carte **ELEGOO ESP-WROOM-32** de l'utilisateur
   (voir ci-dessous — un premier essai avec un NodeMCU-32S 38 broches
   etait incorrect, corrige suite a verification).
2. **2 pastilles pour mini haut-parleur** (face avant).
3. **6 pastilles de prototypage** (face arriere, broches libres du
   controleur).

## Controleur : ESP32 DevKit V1, 30 broches (dos)
2 rangees de **15 broches**, pas 2.54mm, **entraxe 22.86mm (0.9",
"900 mil")**, empreinte ~51.8 x 28.2mm.

**Pastilles traversantes (PTH/percees), pas CMS** — les cartes ELEGOO
ont generalement leurs broches males deja soudees en usine, il faut
donc des trous pour les enficher (et souder cote oppose), pas de
pastilles de surface. La colonne de broches a ete repositionnee (a
DROITE du bouton DROITE plutot qu'au milieu du groupe de boutons) car
des pastilles traversantes ont du cuivre sur F.Cu ET B.Cu meme sur
une broche non utilisee — l'ancien emplacement (au milieu de la
largeur du bouton DROITE) faisait percuter le pont interne du bouton
par une broche du controleur.

**Correction importante** : la 1ere version de ce fichier utilisait le
format "NodeMCU-32S/DevKitC" (38 broches, entraxe 25.4mm/1"), qui est
un AUTRE format d'empreinte ESP32 tres repandu mais different. Apres
verification, les cartes ELEGOO ESP-WROOM-32 (et la plupart des clones
"ESP32 DevKit V1") utilisent le format **30 broches / 15 par rangee /
entraxe 0.9"**, repris ici. Sources verifiees en ligne : dimensions
carte ~51.8 x 28.2mm, 15 broches par rangee, pas 2.54mm standard
("900 mil" = 22.86mm entre rangees). Si ta carte differe (certains
clones ELEGOO existent aussi en 36 ou 38 broches), verifie au pied a
coulisse avant de commander.

**Numerotation generique (broche 1 a 15 par rangee), pas les noms de
silkscreen (GPIOxx) imprimes sur la carte ELEGOO** — il faudra
identifier quelle broche physique (position) correspond a quel GPIO
sur TA carte reelle (le manuel ELEGOO ou une photo annotee de la carte
donnent cette correspondance) avant de souder, et adapter le firmware
en consequence.

- **J1** (rangee gauche) : broche1=DROITE, broche2=GND, broche3=GAUCHE,
  broche4=+3V3, broche5=HP (signal haut-parleur), broches 6-15 libres.
- **J2** (rangee droite) : broches 1-6=PROTO1-6 (prototypage), broche7=GND,
  broche8=+3V3, broche9=SCLK, broche10=MOSI, broche11=RST, broche12=DC,
  broche13=CS, broche14=SAUT, broche15 libre.

## Mini haut-parleur (face avant, 2 pastilles traversantes)
2 pastilles Ø2.5mm (percage Ø1.2mm) pres du centre de la carte, pour
souder directement les 2 fils d'un mini haut-parleur : une va a une
broche GPIO du controleur (signal, via J1 broche5), l'autre a la
masse. **Pas de circuit d'attaque (ampli/transistor) sur ce PCB** —
pour un son audible il faut presque toujours un petit ampli (ex.
PAM8302) ou au minimum un transistor de puissance entre le GPIO et le
haut-parleur ; piloter le haut-parleur en PWM direct depuis le GPIO ne
fonctionne qu'avec de tres petits buzzers piezo basse consommation. A
verifier selon le haut-parleur choisi.

## Pastilles de prototypage (face arriere)
6 broches du controleur (J2, broches 1-6) sont routees vers rien
d'autre que leur propre pastille CMS — elles restent disponibles,
cote dos, pour souder des fils vers d'autres composants au gre des
besoins (capteurs, LED, etc.). Choisies parmi les broches non
utilisees par l'ecran/les boutons/le HP ; **le mapping GPIO reel
depend du module** (meme reserve que ci-dessus).

## Fichiers
- `c3-tft-meatboy-polygon-nodemcu.kicad_pcb` — projet KiCad 10.
  **DRC : 0 erreur, 0 element non connecte.**
- `gerbers/` + `c3-tft-meatboy-polygon-nodemcu-gerbers.zip` — prets a
  envoyer a un fabricant (JLCPCB, PCBWay...), 2 couches, FR4, 1.6mm.

## Notes de routage
- Meme principe "peigne imbrique" que les autres variantes (ordre
  broche/cible croissants ensemble) pour GND/+3V3/SCLK/MOSI/RST/DC/CS.
- **Piege recurrent avec des pastilles traversantes** : contrairement
  aux pastilles CMS (une seule couche), une pastille PTH a du cuivre
  sur F.Cu ET B.Cu — meme non utilisee. Plusieurs trajets ont du etre
  redessines pour ne jamais "reculer" sur la colonne du controleur ni
  friser une pastille de bouton voisine (SAUT notamment, coince entre
  sa propre pastille GND et une broche du controleur). Regle
  generale : sortir immediatement de la colonne (segment court, une
  seule valeur de y) avant toute diagonale ou tout detour.
- Le debut de la colonne de broches est assez proche des pastilles
  ecran en Y (memes coordonnees a ~0.2mm pres par endroits, decalage
  constant du au meme pas 2.54mm des deux cotes) : resolu en placant
  la carte plus bas (Y de depart 32mm) et en choisissant les broches
  7-13 (nettement au-dessus, >=11.5mm, de toutes les cibles J3) pour
  les 7 signaux ecran ; les broches 1-6, plus proches de cette zone,
  servent au prototypage ou elles ne portent aucune piste donc aucun
  risque.
- Le jumper +3V3 (J1.4 -> J2.8) devait eviter les 2 pastilles HP puis
  l'approche laterale de J2 (pas tout droit dans la colonne, qui
  aurait percute la pastille voisine) — 2 detours ajoutes.
- **Pas un seul via sur toute la carte** : les pastilles du
  controleur et de l'ecran sont maintenant toutes traversantes (PTH),
  donc une piste peut partir directement en F.Cu ou B.Cu depuis
  n'importe laquelle sans changement de couche. Seule exception :
  DROITE reste en B.Cu jusqu'a un via juste avant sa pastille de
  bouton CMS (qui, elle, reste F.Cu uniquement).

## AVANT de commander
1. **Module ESP32 DevKit V1 / ELEGOO** : compte les broches sur ta
   carte reelle (doit faire 15 par rangee, 30 au total) et verifie
   l'entraxe (~22.86mm) au pied a coulisse.
2. **Mapping GPIO reel** : voir avertissement ci-dessus (brochage
   generique, pas les noms silkscreen).
3. **Haut-parleur** : verifie s'il te faut un ampli entre le GPIO et
   les pastilles HP (voir section dediee).
4. Boutons 6x6mm/12x12mm et ecran : memes fiches techniques verifiees
   que les versions precedentes (voir `../pcb/README.md`).
