# Brique : l'interface

Page web de l'atelier : décrire un jeu avec l'IA, choisir ses icônes, le publier, le partager par un code et l'envoyer sur la console `c3-tft-meatboy`.

**En ligne :** https://brique-p436.onrender.com (complet) · https://imajon.github.io/ClaudeRepo/brique/ (secours, sans communauté)

## Ce que fait la page

- **Communauté** (accueil) : classement, nouveaux niveaux, joueurs actifs, plébiscités, jeu commun, idées des joueurs, « Récupérer » un jeu par son code.
- **Créer** : trois jeux au choix (plateforme, casse-briques, invaders ; `params.jeu`), parcours de plateforme au choix (zigzag, puits à sauts muraux, escaliers, îles, varié ; `params.forme`), description guidée ou libre, IA (Mistral) qui choisit les réglages, icônes (recherche Iconify, coller, glisser), aperçu, envoi sur la console (Web Serial), publication. Casse-briques et invaders : un seul joueur (pas de course à plusieurs).
- **Animateur** : connexion par lien envoyé par mail, groupes avec code (`LUNE-42`), joueurs, modération des niveaux et des idées.
- **Enfants** : pseudo + code du groupe, aucun mail ni nom.
- **Multi différé** : chaque jeu publié a un code de partage (`K7P2QX`). Chacun le récupère sur sa console quand il veut ; ensemble, l'hôte lance la course dans « 2 à 4 joueurs → Mes jeux ». Les résultats remontent au classement à la connexion suivante.

## Côté navigateur et console

- **Navigateur :** Chrome ou Edge sur ordinateur (Web Serial). Pas Firefox, Safari ni tablette.
- **Adresse :** HTTPS ou `localhost` (une IP du réseau local en `http://` bloque Web Serial).
- **Console :** firmware de ce dépôt (`c3-tft-meatboy`), partitions `huge_app`.
- **Câble :** USB de données. L'IDE Arduino, le moniteur série ou un autre onglet Brique connecté bloquent le port.
- **Réseau de l'école :** laisser passer `api.iconify.design` (recherche d'icônes) et l'adresse Render.

## Serveur (`server.js`)

Node 18 ou plus. `npm install` (une dépendance : `mysql2`), puis `node server.js` (port 8080, ou `PORT=...`).
Sans variables, tout marche en local : données dans `data.json`, liens de connexion affichés dans la console du serveur, IA remplacée par les mots-clés.

| Variable | Rôle |
|---|---|
| `DB_HOST`, `DB_PORT`, `DB_USER`, `DB_PASSWORD`, `DB_NAME` | base MySQL (Aiven) : données gardées entre deux redéploiements |
| `DB_CA` | certificat de la base (connexion chiffrée **et** vérifiée) |
| `BREVO_API_KEY`, `MAIL_FROM` | envoi des liens de connexion animateur (désactiver le blocage d'IP dans Brevo) |
| `MISTRAL_API_KEY` (`MISTRAL_MODEL`) | vraie IA ; sans elle, mots-clés |
| `PUBLIC_URL` | adresse des liens envoyés par mail (sur Render, prise automatiquement) |
| `DATA` | chemin du fichier de données quand il n'y a pas de MySQL |

## Hébergement sur Render

- Web Service sur `Imajon/ClaudeRepo`, Root Directory `brique`, Build `npm install`, Start `node server.js`, variables ci-dessus dans *Environment*.
- Offre gratuite : le service s'endort après 15 minutes sans visite (environ 50 s au réveil) : l'ouvrir quelques minutes avant l'atelier.
- Chaque `git push` sur `main` redéploie automatiquement ; les données restent dans MySQL.
