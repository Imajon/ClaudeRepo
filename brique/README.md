# Brique : l'interface

Page web de l'atelier : décrire un jeu avec l'IA, choisir ses icônes, le publier, le partager par un code et l'envoyer sur la console `c3-tft-meatboy`.

**En ligne :** https://brique-p436.onrender.com (complet) · https://imajon.github.io/ClaudeRepo/brique/ (secours, sans communauté)

## Ce que fait la page

- **Communauté** (accueil) : classement, nouveaux niveaux, joueurs actifs, plébiscités, jeu commun, idées des joueurs, « Récupérer » un jeu par son code.
- **Créer** : quatre jeux au choix (plateforme qui monte, jeu de côté, casse-briques, invaders ; `params.jeu`). Plateforme : 1 à 5 écrans de haut. De côté (`cote`, joué dans le moteur des mondes de la console) : 1 à 4 écrans de large, 1 ou 2 de haut, plafond, trous cachés `T` qui s'ouvrent au dernier moment, porte au bout, parcours de plateforme au choix (zigzag, puits à sauts muraux, escaliers, îles, varié ; `params.forme`), description guidée ou libre, IA (Mistral) qui choisit les réglages, icônes (recherche Iconify, coller, glisser), aperçu, envoi sur la console (Web Serial), publication. Casse-briques, invaders et jeu de côté : un seul joueur (pas de course à plusieurs).
- **Ma fiche** (`params.jeu: "fiche"`) : l'enfant décrit tout son jeu (boutons, saut, briques, objets qui bougent, règles « quand … alors … », but), en partant d'un jeu vide ou d'une copie. Format et état : [FICHE-JEU.md](FICHE-JEU.md) ; code partagé page/serveur : `fiche.js` ; test : `node test-fiche.js`.
- **Animateur** : connexion par lien envoyé par mail, groupes avec code (`LUNE-42`), joueurs, modération des niveaux et des idées.
- **Enfants** : pseudo + code du groupe, aucun mail ni nom.
- **Multi différé** : chaque jeu publié a un code de partage (`K7P2QX`). Chacun le récupère sur sa console quand il veut ; ensemble, l'hôte lance la course dans « 2 à 4 joueurs → Mes jeux ». Les résultats remontent au classement à la connexion suivante.

## Mettre à jour les consoles déjà distribuées

1. Incrémenter `FW_VERSION` dans `c3-tft-meatboy.ino` (ex. `v55.0` → `v55.1`).
2. Lancer `c3-tft-meatboy/publier-firmware-brique.bat` : compile et dépose `brique/firmware/` (4 morceaux + `manifest.json`).
3. Commit + push : Render sert la nouvelle version. En branchant sa console sur Brique, chacun voit « Une nouvelle version est disponible » et clique sur **Mettre à jour ma console** (esp-web-tools, Chrome/Edge).

La progression repart de zéro (voulu : le firmware efface la sauvegarde quand `FW_VERSION` change) ; « Mes niveaux » est gardé. Si la console n’est pas trouvée : brancher en maintenant BOOT.

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
| `STRIPE_SECRET_KEY` | boutique : paiement Stripe Checkout (`sk_test_…` pour tester) ; sans clé en local, paiement simulé |
| `BOUTIQUE_SECRET` | boutique : clé du HMAC des e-mails acheteurs (longue, aléatoire, **ne jamais la changer** : sinon les comptes sont perdus) ; obligatoire en ligne |
| `ADMIN_EMAILS` | boutique : e-mails animateurs qui voient les commandes (séparés par des virgules) |
| `EDIT_CODE` | mode édition des textes de la page : ouvrir `/?edition`, entrer ce code (12 caractères minimum ; 10 codes faux par heure au total, puis blocage d'une heure). Modifs gardées dans les données (MySQL), texte d'origine remis par le bouton « Texte d'origine » ; sans variable : édition désactivée |
| `PUBLIC_URL` | adresse des liens envoyés par mail (sur Render, prise automatiquement) |
| `DATA` | chemin du fichier de données quand il n'y a pas de MySQL |

En local, au démarrage, le serveur copie toutes les images, vidéos, PDF et documents de `brique/` (sauf `versions/`, `firmware*`, `node_modules/`) dans `_images/C3-Brique/Selection Brique Web/` (mêmes sous-dossiers, seulement les fichiers nouveaux ou changés). Sur Render ce dossier n'existe pas : rien n'est copié.

## Hébergement sur Render

- Web Service sur `Imajon/ClaudeRepo`, Root Directory `brique`, Build `npm install`, Start `node server.js`, variables ci-dessus dans *Environment*.
- Offre gratuite : le service s'endort après 15 minutes sans visite (environ 50 s au réveil) : l'ouvrir quelques minutes avant l'atelier.
- Chaque `git push` sur `main` redéploie automatiquement ; les données restent dans MySQL.
