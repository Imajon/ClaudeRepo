# Brique : l'interface

Page web de l'atelier : décrire un jeu, le tester, choisir ses icônes et l'envoyer sur la console `c3-tft-meatboy`.

**En ligne :** https://imajon.github.io/ClaudeRepo/brique/

## Deux façons de l'utiliser

| | GitHub Pages | `node server.js` |
|---|---|---|
| Créer et tester un jeu | oui | oui |
| Icônes (recherche, coller, glisser) | oui | oui |
| Envoyer sur la console (Web Serial) | oui | oui |
| Pseudo, publication, classement, jeu commun | non | oui |

Le classement a besoin de `server.js`. Sans lui, la page fonctionne quand même, mais sans la partie en ligne.

## Configurations nécessaires

- **Navigateur :** Chrome ou Edge sur ordinateur. Web Serial ne marche ni dans Firefox, ni dans Safari, ni sur tablette.
- **Adresse :** il faut du HTTPS ou `localhost`. GitHub Pages et `http://localhost:8080` conviennent ; une adresse IP du réseau local (`http://192.168…`) ne marche pas.
- **Console :** le firmware `c3-tft-meatboy` doit être celui de ce dépôt (lecture série, Mes niveaux, icônes), compilé avec le schéma de partitions `huge_app`.
- **Câble :** un câble USB de données, pas un câble de charge seule. L'IDE Arduino et le moniteur série doivent être fermés, car ils bloquent le port.
- **Pilote :** aucun sous Windows 10 et 11, l'ESP32-C3 est vu comme un port COM USB.
- **Internet :** seulement pour la recherche d'icônes, qui passe par `api.iconify.design`. Un filtre réseau d'école doit laisser passer ce domaine.
- **Serveur de classement :** Node 18 ou plus, sans dépendance. Lancer `node server.js`, puis ouvrir `http://localhost:8080`. Le port se change avec `PORT=...`. Les données sont dans `data.json`, qui n'est pas versionné ; leur emplacement se change avec `DATA=...`.

## Hébergement sur Render

- Créer un « Web Service » sur le dépôt `Imajon/ClaudeRepo`, avec Root Directory `brique`, aucune commande de build et la commande de démarrage `node server.js`.
- Offre gratuite : le service s'endort après 15 minutes sans visite (environ 50 secondes au réveil) et `data.json` est effacé à chaque redéploiement.
- Pour garder le classement : une instance payante avec un disque monté sur `/var/data`, et la variable d'environnement `DATA=/var/data/data.json`.
- Chaque `git push` sur `main` redéploie automatiquement.
