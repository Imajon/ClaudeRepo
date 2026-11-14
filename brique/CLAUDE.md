# Brique — contexte pour Claude

Site de l'atelier : les enfants (9-14 ans) décrivent un jeu, l'IA (Mistral) le règle ou l'écrit, le jeu part sur la console `c3-tft-meatboy` par Web Serial. Détails utilisateur : [README.md](README.md). Ne pas relire `../CONTEXT-interface-Brique.md` (note de départ, dépassée).

## Fichiers vivants (les seuls à lire)
- `index.html` (~3000 l.) : toute la page. **Ne jamais lire en entier** : chercher la section par son repère `// ---------- <titre> ----------` (JS, à partir de ~l.1396) ou `<!-- ===== <VUE> ===== -->` (HTML), puis lire autour.
  - Vues HTML : ACCUEIL, COMMUNAUTÉ, BOUTIQUE, SUPPORT, ATELIER, ESPACE ANIMATEUR, REJOINDRE UN GROUPE, CRÉER (demande / console / historique), CRÉER (mode script).
  - JS : générateur de niveau (format firmware), Ma fiche, icônes, Web Serial, mise à jour console (esp-web-tools), communauté, animateur, boutique/Stripe, mode script, vues.
- `server.js` (~750 l.) : Node sans framework, routes `/api/...` (ia, script, choix = les 3 boutons de « 💡 Aide-moi » en tranche guidée, jeu, niveaux, communauté, animateur/*, client/* = boutique, parties, courses, idées, questions). Données : MySQL si `DB_*`, sinon `data.json`.
- `fiche.js` + `FICHE-JEU.md` : format « Ma fiche », partagé page/serveur. Test : `node test-fiche.js`.
- `consigne-script.txt` : prompt système du mode script (Berry), sans les exemples.
- `systemes/*.be` + `exemples.js` : un système de jeu par fichier (saut, tirs, jauge, radio...) ; l'IA reçoit les 3 plus proches de la demande (mots-clés de la 2e ligne `# exemple: <titre> | mots: a, b`). Ajouter un système = un fichier `.be` + `node test-exemples.js` (le joue dans verif). `exemples/*.be` (jeux entiers) ne sert plus qu'au banc d'essai.
- `bench-exemples.js` : banc d'essai jeux entiers contre systèmes (Mistral génère, verif joue, un juge compare à l'aveugle) ; `MISTRAL_API_KEY=... node bench-exemples.js`, résultats dans `bench/` (non versionné).
- `en.js` + `en.json` : la version anglaise, servie sur `/en` (lien EN / FR de l'en-tête). `en.js` découpe `index.html` en segments (HTML et chaînes du JS) et remplace ceux qui ont une entrée dans `en.json` (clé = le texte français ; `{1}`, `{2}`… = une balise à attributs ou un `${…}`, à reprendre tels quels ; clé = valeur : à laisser tel quel). La page anglaise envoie `X-Lang: en` : `server.js` traduit alors ses messages (`tr()`, même dictionnaire), le mail, Stripe, et demande à l'IA de répondre en anglais (`EN_IA`). Les `mots:` des `systemes/*.be` ont aussi des mots anglais. Restent en français : `fiche.js`, le repli par mots-clés sans IA, le firmware de la console.
  - **Après avoir changé ou ajouté un texte français : `node en.js`** (liste les textes sans traduction et les entrées qui ne servent plus, vérifie que les scripts de la page anglaise compilent), puis compléter `en.json`.
  - Ne pas traduire un mot seul qui sert aussi de clé dans le code (`'trous'`, `'plafond'`…) : la même entrée s'applique partout. Pas de `'`, `"` ni `` ` `` dans une traduction : `’` et `“ ”`.
- `version.txt` : numéro du site (`75` → affiché v1.75).
- `bump-sections.js` : au commit, monte le `data-version` de chaque `<main>` touché (affiché « section v1.x » en pied de page). Hook à installer une fois par clone : `node brique/bump-sections.js --installer`.

## À ne jamais lire (ou seulement sur demande)
- `versions/`, `index-V*.html`, `server-avant-*.js` : anciennes versions, servent de sauvegarde.
- `firmware/`, `firmware-berry/` (binaires générés), `images/`, `instagram/`, `node_modules/`, `package-lock.json`, `data.json`.

## Côté console
- Firmware jeux : `../c3-tft-meatboy/c3-tft-meatboy.ino` (~9100 l.) : chercher par nom de fonction, ne pas lire en entier. Les `*_sprites.h` sont des données, ne pas les ouvrir.
- Firmware script (Berry) : `../c3-berry/c3-berry.ino`.
- Publier une mise à jour : incrémenter `FW_VERSION` dans le `.ino`, lancer `../c3-tft-meatboy/publier-firmware-brique.bat` (dépose `firmware/`), commit + push.

## Lancer / déployer
- Local : `node server.js` (port 8080). Sans variables : `data.json`, IA remplacée par des mots-clés.
- En ligne : Render redéploie à chaque push sur `main` (https://brique-p436.onrender.com).

## Règles
- Avant de réécrire `index.html` ou `server.js` en profondeur : copier dans `versions/<nom>-avant-<sujet>.*`.
- Enfants : aucune donnée personnelle (pseudo + code de groupe seulement).
- Commits en français : `Brique : <ce qui change>`.
