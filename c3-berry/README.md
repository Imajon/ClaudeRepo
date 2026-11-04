# c3-berry : le jeu est un script (preuve de faisabilité)

La console `c3-tft-meatboy` fait tourner un script [Berry](https://github.com/berry-lang/berry) (licence MIT, dans `src/berry/`) reçu par le câble. Il n'y a pas de compilation et le firmware reste intact. C'est la piste du mode expert de Brique : l'IA écrit le jeu, et les seules limites sont celles de l'ESP32-C3.

## Envoyer un jeu

```
python envoyer.py jeux/drift2.be COM23
```

La console répond `OK` ou `ERR <erreur, ligne>`. `envoyer.py` demande aussi les mesures (`STAT 1`) : une ligne par seconde (images/s, temps du script, temps du rendu, pire image, nettoyages de la mémoire, tas libre). `python conduire.py COM23` mesure le jeu chargé pendant qu'on « conduit » (`BTN 011` = D et P3 tenus). Le jeu est gardé dans `/jeu.be` et relancé au démarrage, sauf si la console vient de planter.

Compiler et flasher : comme `c3-tft-meatboy` (même carte, `PartitionScheme=huge_app`, bibliothèques de `../c3-tft-meatboy/libs`).

## Ce que le script écrit

- `debut()` : appelée une fois au lancement.
- `maj(dt)` : la logique du jeu (dt en secondes).
- `dessine()` : les appels de dessin.

## Ce que la console offre

- **Dessin** : `fond(c)`, `rect`/`cadre(x,y,l,h,c)`, `disque`/`cercle(x,y,r,c)`, `ligne(x1,y1,x2,y2,c)`, `tri(...,c)`, `texte(x,y,s,c,taille)`, `rgb(r,g,b)`, `camera(x,y)`.
- **Images** (b5) : `image(n, x, y, anim, zoom, miroir)` dessine l'image n (1, 2, 3...) faite sur Brique à partir de n'importe quelle image ou GIF : 16 couleurs (0 = transparent), plusieurs images d'animation (la console boucle sur `anim`), zoom 1 à 8, miroir horizontal. Elles voyagent avec le script : `SCRIPT <octets>` couvre le script, un octet 0, puis les images (format dans `chargeImages`). Budget : 32 Ko de pixels par jeu, lus sans copie dans le fichier du jeu. Test : `python envoyer-images.py COM24` (un fond 160×120 en zoom 2 et un personnage 48×48 animé).
- **Boutons** : `bouton(k)` (tenu), `appui(k)`, `lache(k)`, avec `k` = `G`, `D` ou `P3`.
- **Autres** : `temps()`, `hasard(a,b)`, `son(freq,ms)`.
- **Réseau** (ESP-NOW, toutes les consoles autour) : `envoie(chaine)`, `recois()` qui renvoie `[[id, chaine], ...]`, `moi()`.
- **Modules Berry** : `math`, `string`.

Le dessin est enregistré une seule fois par image, puis rejoué pour chacune des 4 bandes de 60 lignes. Une bande part à l'écran en DMA pendant que le CPU dessine la suivante.

## Mesures (1er novembre 2026, console sur COM23)

| Script | Images/s | Script | Remarque |
|---|---|---|---|
| `vide.be` (un carré) | 63 | 0,13 ms | Plafond de l'écran : 15,7 ms par image en SPI à 80 MHz. Sans DMA : 43 images/s. |
| `timing.be` | 62 | 0,9 ms | Réseau allumé. |
| `drift.be` (écrit naïvement) | 45 | 13 ms | 24 segments, racines carrées à chaque image. |
| `drift2.be` (même jeu, optimisé) | 59 | 6 ms | Bords précalculés, seulement les segments voisins. |
| `test-reseau.be` | 62 | 1,8 ms | 63 messages ESP-NOW/s envoyés pendant le DMA, aucun refusé. |
| `test-recoit.be` à 2 consoles (COM23 et COM24) | 62 | 1,5 ms | Chacune reçoit environ 20 messages/s de l'autre, soit tout ce qui est envoyé. |
| `drift2.be` à 2 consoles | 59 | 6,5 ms | Chacune dessine la voiture de l'autre (58 appels de dessin au lieu de 57). Aucun plantage sur 3 rechargements. |
| `test-boucle.be` | — | — | Boucle sans fin arrêtée au bout de 0,5 s, avec la ligne fautive ; la console continue. |

- **Flash** : Berry prend au plus 143 Ko. Le sketch entier (Wi-Fi et ESP-NOW compris) fait 1 Mo, soit 32 % de la partition.
- **Mémoire** : il reste 145 Ko de tas sans réseau, et 85 à 100 Ko avec le réseau et Drift.
- **Compilation** : Berry en `-O2` au lieu de `-Os` ne fait gagner que 4 %. Le coût vient du code du script (calcul en virgule flottante logiciel : le C3 n'a pas d'unité de calcul flottant).

- **Pile** : 16 Ko pour la tâche principale. Avec les 8 Ko par défaut, la pile a débordé au chargement de Drift, sur une console qui avait déjà le réseau allumé. Après ce plantage, la console a redémarré sans relancer le jeu, comme prévu.

- **Port USB** : le C3 bloquait le jeu à chaque écriture quand aucun programme ne lisait le port. Le jeu tombait vers 2 à 10 images/s à l'écran alors que les tests, port lu, mesuraient 59. Désormais l'écriture ne bloque plus (`setTxTimeoutMs(0)`) et les mesures ne s'écrivent que sur demande. Vérifié à l'œil sur les 2 consoles : fluide.
- **En conduisant** (`conduire.py`) : `drift2.be` passe de 59 à 51 images/s quand les traces de dérapage s'accumulent (110 carrés par image), avec une pire image à 25 ms. Les nettoyages de la mémoire de Berry restent sous 3,4 ms. Les traces sont à alléger dans le script.
