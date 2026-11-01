# La fiche de jeu Brique

> **État (v56.0, 1er novembre 2026)** : la fiche marche de bout en bout : page (« 📋 Ma fiche »), IA, serveur et firmware. Les 6 parties sont lues par la console ; les écarts avec la proposition d'origine sont signalés « ⚠️ » ci-dessous.
> Code : `fiche.js` (vérification, lecture en français, aperçu ; partagé par la page et `server.js`), `bqFicheParse()` dans `c3-tft-meatboy.ino`. Test : `node test-fiche.js`.

## L'idée

Aujourd'hui, l'IA choisit des **réglages**, et la console fait tourner un jeu déjà écrit. Avec la fiche, l'enfant **décrit tout son jeu** :

- ce que font les boutons ;
- comment le héros saute ;
- ce que font les briques et les obstacles ;
- ce qui arrive quand on touche un ennemi ;
- comment on gagne.

La console ne connaît plus « le jeu de plateforme ». Elle connaît un **moteur** qui lit la fiche et applique ce qui est écrit, rien de plus.

On peut partir de deux points de départ :

| Point de départ | Ce que l'enfant a au début |
|---|---|
| **Jeu vide** | Un carré posé sur un sol. Les boutons ne font rien. Rien ne marche tant qu'il ne l'a pas expliqué. |
| **Copie d'un jeu** (plateforme, de côté, iso…) | La fiche complète du jeu, avec sa physique. Il change seulement ce qu'il veut : « saute deux fois plus haut ». |

La fiche voyage comme les niveaux actuels : une ligne JSON par le câble USB, de 3600 octets au plus. Les jeux actuels restent dans le firmware, inchangés.

## Les unités : celles d'un enfant

Tout se compte en **cases** (une case = 16 pixels) et en **secondes**. Le firmware fait la conversion.

Exemples : « saute de 4 cases », « avance de 12 cases par seconde », « la scie fait l'aller en 2 secondes ».

## Les 6 parties de la fiche

```
monde    taille, gravité, ce qui se passe au bord
heros    boutons, vitesse, saut, murs, vies
briques  ce que fait chaque lettre de la grille
objets   ce qui bouge : trajet, vitesse, déclencheur, contact
regles   « quand … alors … »
but      comment on gagne
```

En plus, `rows` dessine la grille, comme aujourd'hui. Chaque lettre utilisée doit être décrite dans `briques` ou `objets`, sinon la page dit « Je ne sais pas ce qu'est la lettre K ».

### 1. `monde`

| Clé | Valeurs | Défaut (jeu vide) |
|---|---|---|
| `gravite` | 0 à 200 cases/s² (0 = vue de dessus, rien ne tombe) | 0 |
| `bord` | `mur` (le bas est un sol), `vide` (tomber en bas = perdre ; les côtés restent des murs, comme aujourd'hui), `boucle` (on ressort de l'autre côté, et du bas vers le haut) | `vide` ⚠️ |
| `camera` | `suit` le héros, ou `fixe` | `suit` |

La taille est celle de `rows`.

### 2. `heros`

| Clé | Valeurs | Défaut (jeu vide) |
|---|---|---|
| `boutons` | Une action pour `G`, `D` et `P3` (liste ci-dessous) | `rien`, `rien`, `rien` |
| `vitesse` | 0 à 20 cases/s | 0 |
| `elan` | 0 à 1 (0 = s'arrête net, 1 = glisse longtemps ; 0,2 = la plateforme d'aujourd'hui) | 0,2 |
| `saut` | `hauteur` (cases), `en_l_air` (0, 1 ou 2 sauts de plus) | aucun |
| `mur` | `glisse` (oui/non), `saut_mural` (oui/non) | non, non |
| `vies` | 0 = on recommence à chaque fois, 1 à 9 | 0 |

Les actions possibles pour un bouton :

- `aller_gauche`, `aller_droite`, `aller_haut`, `aller_bas` : tant qu'on appuie ;
- `sauter` ;
- `tourner_gauche`, `tourner_droite`, `avancer_case` : déplacement case par case, comme dans le jeu iso ;
- `tirer` : lance l'objet `projectile` décrit dans `objets` (sans projectile décrit : 10 cases/s) ; il casse les ennemis et ce qu'on peut écraser ;
- `rien`.

⚠️ La forme du héros vient de la page (héros et icônes), pas de la fiche.

### 3. `briques` : ce que fait chaque lettre de la grille

Chaque lettre combine des **propriétés**. C'est là que l'enfant invente : un pic qui fait rebondir, une glace mortelle…

| Propriété | Sens |
|---|---|
| `solide` | On marche dessus, on ne passe pas à travers. |
| `mortel` | La toucher fait perdre. |
| `glisse` | 0 à 1 : on garde son élan. |
| `rebond` | Hauteur du rebond, en cases. |
| `disparait` | Secondes après qu'on a marché dessus (bloc fragile). |
| `ramasse` | Disparaît quand on la touche et compte comme un objet ramassé (les 3 premières sont les étoiles du moteur, les suivantes des pièces). |
| `arrivee` | La toucher peut faire gagner (voir `but`). |

⚠️ La couleur des cases vient de la palette de la page. Une lettre sans propriété est du décor : on passe à travers (pratique pour un « bouton » de règle `touche`).

### 4. `objets` : tout ce qui bouge ou se déclenche

Chaque objet est posé dans la grille par une lettre et décrit ici. Il y a 16 objets au plus dans un niveau, toutes lettres comprises.

| Clé | Valeurs |
|---|---|
| `forme` | `pic`, `scie`, `bloc`, `ennemi`, `projectile` (l'icône « ennemi » de la page remplace le dessin des ennemis) |
| `mouvement` | `fixe`, `aller_retour`, `boucle` (fait le tour de ses points), `tomber`, `suivre` (va vers le héros), `patrouille` (marche sur sa plateforme) |
| `points` | Pour `aller_retour` et `boucle` : décalages en cases depuis la position de départ, par exemple `[[0,0],[5,0]]` |
| `duree` | Secondes pour aller d'un point au suivant |
| `vitesse` | Cases par seconde pour `suivre` (3), `patrouille` (2) et `projectile` (10) |
| `pause` | Secondes d'arrêt à chaque point |
| `declencheur` | `toujours`, `proche` (avec `distance` en cases), `touche` |
| `cache` | oui = invisible tant qu'il n'est pas déclenché (le pic caché d'aujourd'hui) |
| `contact` | Ce qui arrive au héros qui le touche : `perd`, `rebond`, `ramasse`, `vie_moins`, `rien` |
| `dessus` | Ce qui arrive quand on lui saute dessus : `ecrase` (il disparaît, on rebondit), ou comme `contact` |

Un `bloc` avec `contact: rien` qui bouge est une **plateforme mobile** : on tient dessus et elle nous emporte. `touche` démarre l'objet quand on arrive à une case de lui.

C'est cette partie qui répond à « un obstacle qui bouge exactement comme je le décris ».

### 5. `regles` : « quand … alors … »

Il y a 8 règles au plus. Elles servent pour ce que les briques et les objets ne disent pas.

| Quand | Alors |
|---|---|
| `touche` une lettre | `gagne` |
| `ramasse_tout` | `perd` |
| `ecrase_tout` | `ouvre` une lettre (elle devient vide) |
| `temps` (secondes) | `ferme` une lettre (elle devient solide) |
| `points` atteints (nombre) | `vie_plus` |
| | `message` (un texte) |
| | `vitesse` (multiplie la vitesse du héros) |

Les deux colonnes se combinent librement : n'importe quel « quand » avec n'importe quel « alors ». Écriture : un mot seul, ou le mot et sa valeur :

```json
"regles":[{"quand":{"touche":"K"},"alors":{"ouvre":"P"}},
          {"quand":"ramasse_tout","alors":{"message":"La porte est ouverte"}},
          {"quand":{"temps":60},"alors":"perd"}]
```

Chaque règle joue une fois par vie ; à chaque vie, les briques ouvertes ou fermées reviennent. Les `points` comptent ce qu'on a ramassé et écrasé. Le message fait 20 caractères au plus, sans accents.

### 6. `but`

Valeurs possibles : `arriver` (toucher une brique `arrivee`), `ramasser_tout`, `ecraser_tout`, `{"survivre": 30}` (secondes), ou `regles` (seules les règles font gagner).

## Exemple 1 : le jeu vide

```json
{"jeu":"fiche","rows":["..........","..S.......","##########"],
 "monde":{"gravite":0},
 "heros":{"boutons":{"G":"rien","D":"rien","P3":"rien"}},
 "briques":{"#":{"solide":1}},
 "objets":{},"regles":[],"but":"arriver"}
```

Sur la console, le carré ne bouge pas. Sur la page, la fiche se lit en français et dit ce qui manque :

> 🟥 Ton héros est un carré.
> ❓ Quand j'appuie sur G, D ou P3 : **rien ne se passe**. Explique ce que font les boutons !
> ❓ Il n'y a pas d'arrivée : comment on gagne ?

## Exemple 2 : la plateforme d'aujourd'hui, écrite en fiche

Les valeurs viennent du firmware actuel :

- gravité 1400 px/s², soit 87,5 cases/s² ;
- course 200 px/s, soit 12,5 cases/s ;
- saut 450 px/s, soit 4,5 cases de haut ;
- saut mural ;
- ressort 560 px/s, soit 7 cases ;
- ennemis écrasés par-dessus.

Le moteur de fiche devra rejouer les niveaux actuels **à l'identique** avec cette fiche : c'est le test de départ.

```json
{"jeu":"fiche","modele":"plateforme",
 "rows":["...","(la grille, comme aujourd'hui)","..."],
 "monde":{"gravite":87.5,"bord":"vide","camera":"suit"},
 "heros":{"boutons":{"G":"aller_gauche","D":"aller_droite","P3":"sauter"},
          "vitesse":12.5,"elan":0.2,"saut":{"hauteur":4.5,"en_l_air":0},
          "mur":{"glisse":1,"saut_mural":1},"vies":0,"forme":"carre"},
 "briques":{"#":{"solide":1},
            "I":{"solide":1,"glisse":0.9},
            "C":{"solide":1,"disparait":0.5},
            "R":{"solide":1,"rebond":7},
            "^":{"mortel":1},
            "*":{"ramasse":1},
            "G":{"arrivee":1}},
 "objets":{"W":{"forme":"scie","mouvement":"aller_retour","points":[[0,-3],[0,0]],"duree":1.2,"pause":0,"declencheur":"toujours","contact":"perd"},
           "E":{"forme":"ennemi","mouvement":"patrouille","duree":2,"declencheur":"toujours","contact":"perd","dessus":"ecrase"},
           "H":{"forme":"pic","mouvement":"fixe","declencheur":"proche","distance":2,"cache":1,"contact":"perd"}},
 "regles":[],
 "but":"arriver"}
```

Cette fiche fait environ 0,9 Ko, sans la grille. La grille d'un niveau de 2 écrans en fait environ 0,7 Ko : le tout tient dans les 3600 octets.

## Exemple 3 : « une scie qui va de gauche à droite en 2 secondes, attend 1 seconde, et revient »

L'IA ajoute un objet, sans rien toucher d'autre :

```json
"objets":{"Z":{"forme":"scie","mouvement":"aller_retour","points":[[0,0],[6,0]],
               "duree":2,"pause":1,"declencheur":"toujours","contact":"perd"}}
```

Elle pose aussi un `Z` dans la grille, à l'endroit voulu. La page relit l'objet en français :

> ⚙️ Une scie va de 6 cases vers la droite en 2 s, attend 1 s et revient. La toucher fait perdre.

## Ce que fait l'IA

1. Elle reçoit la fiche actuelle et la phrase de l'enfant.
2. Elle renvoie **seulement les changements**, par exemple `{"heros":{"saut":{"hauteur":9}}}`.
3. Le serveur vérifie chaque changement : clés connues, valeurs dans les bornes, 16 objets et 8 règles au plus. Il refuse le reste et le dit à l'enfant.
4. La page fusionne les changements, réaffiche la fiche en français et l'aperçu, et liste ce qui manque ou se contredit. Exemples : « le but est de tout ramasser mais il n'y a rien à ramasser », « le saut fait 2 cases mais la première marche en fait 3 ».
5. Ce que la fiche ne sait pas faire part dans les « idées des joueurs », comme aujourd'hui.

## Les limites de la console

- Mémoire disponible : environ 59 Ko. La fiche décodée prend moins de 2 Ko :
  - 16 objets × environ 48 octets ;
  - 16 lettres de briques × 8 octets ;
  - 8 règles × 8 octets.
- Grille de 20 × 80 cases au plus (plateforme), ou 96 × 30 (de côté).
- Rendu : le même que le jeu de plateforme actuel. Les formes d'objets sont dessinées par le firmware ; une icône les remplace.

## Ce qui est fait (v56.0)

| Étape | État |
|---|---|
| 1. Moteur de fiche, plateforme | ✅ La fiche « Copie de la plateforme » se traduit lettre pour lettre dans les briques d'aujourd'hui (`#ICR^*GWEH`). La scie `W` devient un objet de la fiche (même trajet, vitesse très proche). |
| 2. Page | ✅ Jeu « 📋 Ma fiche » : jeu vide ou copie (plateforme, côté, iso), fiche lue en français, ce qui manque (❓) et ce qui empêche l'envoi (⛔), mode expert (la fiche en JSON), aperçu, envoi, publication. |
| 2. IA + serveur | ✅ Mistral renvoie seulement les changements (`changements`, `poser`) ; `fiche.js` vérifie tout côté serveur ; « compris » = les phrases nouvelles de la fiche ; l'impossible part dans les idées des joueurs. |
| 3. Objets | ✅ `boucle`, `tomber`, `suivre`, `patrouille` (avec les contacts de la fiche), projectiles, `touche`, `cache`, `ramasse`, `vie_moins`, plateformes mobiles ; `bord` et `camera`. |
| 4. Règles et buts | ✅ Les 5 « quand », les 7 « alors », les 5 buts. |
| 5. Côté et iso | ✅ `"modele":"cote"` / `"iso"` : la grille est traduite pour ces moteurs, avec vitesse, gravité, saut et boutons de la fiche. ⚠️ Pas d'objets qui bougent (sauf les ennemis qui patrouillent en côté, et les pics qui sortent en iso), pas de règles, but = arriver : la page le dit avant l'envoi. |

Pas encore vérifié : la jouabilité à l'écran (les fiches de test passent sur la console, il reste à y jouer), et l'IA réelle avec la clé Mistral (testée avec une réponse simulée).

## Questions pour toi

1. **Jeu vide** : le héros est-il vraiment immobile au départ, même sans `aller_gauche` ni `aller_droite` ? C'est le plus pédagogique.
2. **Noms des propriétés** : on garde des mots français simples (`mortel`, `rebond`, `ecrase`) que l'enfant peut lire dans la fiche ? Faut-il pouvoir montrer la fiche brute aux 6ᵉ–4ᵉ, en « mode expert lecture » ?
3. **Priorité** : on commence bien par la plateforme (étape 1) avant les autres modèles ?
