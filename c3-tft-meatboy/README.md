# Brique — la console de jeu que les enfants soudent eux-mêmes

<p align="center"><img src="docs/pcb-console.jpg" width="480"></p>

**Brique** est une petite console de jeu portable à base d'**ESP32-C3** et d'un **écran TFT 2"**, conçue pour des ateliers avec des enfants (CM1 → 4ᵉ). Chaque enfant monte et soude sa propre console, puis joue à **MEAT RUN**, un jeu de plateforme façon *Super Meat Boy* écrit pour la console.

Prochaine étape : laisser les enfants **créer leurs propres niveaux et règles avec une IA**, depuis un navigateur, sans rien installer.

---

## Le concept

<p align="center">
  <img src="docs/croquis-console.jpg" width="45%">
  <img src="docs/schema-console-hex.jpg" width="45%">
</p>

Trois composants, trois boutons, un fer à souder. L'objectif : un objet simple à assembler, mais qui donne un vrai jeu fluide une fois allumé.

## Le matériel

| Composant | Rôle |
|---|---|
| ESP32-C3 Super Mini | Microcontrôleur Wi-Fi / ESP-NOW, USB-C |
| Écran TFT 2" ST7789 (SPI, 320×240) | Affichage |
| 3 boutons-poussoirs | Gauche (GPIO2), Droite (GPIO1), Saut (GPIO3) |
| Plaque à pastilles ou PCB hexagonal | Support |

<p align="center">
  <img src="docs/esp32-c3.jpg" width="32%">
  <img src="docs/ecran-tft.jpg" width="32%">
  <img src="docs/bouton.jpg" width="32%">
</p>

## L'atelier

Les enfants partent des composants en vrac et repartent avec une console qui marche.

<p align="center">
  <img src="docs/atelier-composants.jpg" width="45%">
  <img src="docs/atelier-ecran-module.jpg" width="45%">
</p>
<p align="center">
  <img src="docs/atelier-plaque.jpg" width="24%">
  <img src="docs/atelier-montage.jpg" width="24%">
  <img src="docs/atelier-soudure.jpg" width="24%">
  <img src="docs/atelier-soudure-2.jpg" width="24%">
</p>

## Version 1 — prototype sur plaque à pastilles

Câblage fil à fil au dos, écran et boutons en façade.

<p align="center">
  <img src="docs/proto-cablage.jpg" width="45%">
  <img src="docs/console-proto.jpg" width="45%">
</p>
<p align="center">
  <img src="docs/proto-ecran-titre.jpg" width="30%">
  <img src="docs/proto-jeu.jpg" width="45%">
</p>

## Version 2 — PCB hexagonal

Un PCB dédié en forme de briques hexagonales : moins de fils, montage plus rapide en atelier.

<p align="center">
  <img src="docs/pcb-dessin.jpg" width="45%">
  <img src="docs/pcb-rendu.jpg" width="45%">
</p>
<p align="center">
  <img src="docs/pcb-dos.jpg" width="30%">
  <img src="docs/pcb-jeu.jpg" width="30%">
  <img src="docs/pcb-console.jpg" width="30%">
</p>

Fichiers KiCad et SVG du boîtier : [`materiel/`](materiel/).

## Le jeu : MEAT RUN

- **100 niveaux** variés (ressorts, pics, blocs tombants, ennemis, scies…), étoiles à collecter, drapeau final.
- **Mode 2 joueurs** sans fil (ESP-NOW) : course avec le fantôme de l'adversaire.
- **Mini-jeux** bonus toutes les 10 étapes : Pong, Casse-brique, Invaders.
- **Quiz CM2** (maths, français, anglais) entre les niveaux, avec récompenses visuelles.
- **Classement partagé** entre consoles, boutique de skins, mascotte « Steak ».
- Progression sauvegardée en mémoire flash.

## Compiler et flasher

Sketch Arduino : [`c3-tft-meatboy.ino`](c3-tft-meatboy.ino) (carte ESP32C3 Dev Module).

```bat
compiler-upload-c3-tft-meatboy.bat
```

Détails techniques et historique des versions : [`CONTEXTE.md`](CONTEXTE.md).

## La suite : créer son jeu avec l'IA

Idée : un firmware fixe (moteur, affichage, sons) et une IA qui génère uniquement **les niveaux et les règles** sous forme de données, envoyées à la console en quelques millisecondes depuis une page web. Le but n'est pas d'apprendre la syntaxe, mais de montrer aux enfants qu'ils peuvent **décrire, tester et corriger** un jeu qu'ils ont imaginé.

---

*AntiFaktory — 2026*
