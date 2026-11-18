# « Ma fiche » : ce qui reste à essayer sur la console

Le banc `node bench-fiche.js` (site lancé avec `lancer-brique.bat ia`) vérifie déjà, avec la vraie IA, que la **fiche obtenue est la bonne** (objet, lettre posée dans la grille, durées, règle). Il ne voit pas le **jeu qui tourne** : c'est cette liste.

Mode d'emploi : page `http://localhost:8080/#creer` → bouton **📋 Fiche** → « Un jeu vide » (sauf mention) → tape la phrase → regarde la fiche lue → envoie sur la console (Chrome ou Edge, USB, firmware v56.1). Coche, et note ce qui diffère.

| ✔ | Essai | Phrase à taper | À voir sur la console | Fiche vérifiée par le banc |
|---|---|---|---|---|
| ✅ | 1. Copie plateforme | (départ « Copie de la plateforme ») | se joue comme le vrai jeu | — |
| ✅ | 2. Mouvement | « G et D déplacent, P3 saute » | G/D bougent, P3 saute, sol sous les pieds | oui |
| ☐ | 3a. Scie | « une scie qui va de gauche à droite en 2 secondes, attend 1 seconde, et revient » | va-et-vient en 2 s, 1 s d'arrêt à chaque bout (chronomètre), la toucher fait perdre | oui |
| ☐ | 3b. Ennemi | « un ennemi qui me suit » | il se rapproche du héros, le toucher fait perdre | oui |
| ☐ | 3c. Plateforme | « une plateforme qui bouge de gauche à droite » | on monte dessus **et elle emporte le héros** | oui (pas l'emport) |
| ☐ | 4a. Clé et porte | « une clé K ouvre une porte P » | toucher K → la porte disparaît, on passe | oui (pas l'effet) |
| ☐ | 4b. Temps | « si je mets plus de 60 secondes, je perds » | après 60 s : défaite (essayer avec 10 s pour aller vite) | oui |
| ☐ | 5a. Saut | « mon héros saute deux fois plus haut » | le saut est visiblement ~2× plus haut | oui (valeur) |
| ☐ | 5b. Enlever | après 3a : « enlève la scie » | la scie a disparu de l'écran | oui |
| ☐ | 6. Cumul | scie, puis ennemi, puis plateforme, puis clé/porte, dans la même fiche | les 4 coexistent, aucun n'écrase l'autre à l'écran | oui |
| ☐ | 7. Côté | départ « Copie du jeu de côté » | se joue comme le vrai ; pas d'objets mobiles ni de règles (la page le dit avant l'envoi) | non |
| ☐ | 8. Iso | départ « Copie de l'isométrique » | idem, le moins sûr | non |
| ☐ | 9. Impossible | « un dragon qui vole et crache du feu » | rien de cassé ; la page dit ce qui est impossible (💡 idées) | oui (pas de plantage) |
| ☐ | 10. Message | une règle avec message de victoire accentué | texte lisible, sans accents cassés (la console n'a pas d'accents) | non |

À noter pour chaque ligne : ça se joue ou non · ce qui diffère de la phrase · tout plantage ou « ⛔ » inattendu.

## Limites connues du banc
- L'IA varie d'un passage à l'autre : le banc répète chaque scénario (`node bench-fiche.js 3`) et affiche « n/3 ». Un ⚠️ isolé (2/3) est du bruit de l'IA ; un ❌ est un vrai défaut.
- Le banc part toujours du jeu vide (héros déjà mobile) : les trois copies (plateforme, côté, iso) sont à essayer à la main.
